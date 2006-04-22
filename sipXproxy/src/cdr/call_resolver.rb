#
# Copyright (C) 2006 SIPfoundry Inc.
# Licensed by SIPfoundry under the LGPL license.
# 
# Copyright (C) 2006 Pingtel Corp.
# Licensed to SIPfoundry under a Contributor Agreement.
#
##############################################################################

# system requires
require 'rubygems'            # Ruby packaging and installation framework
require_gem 'activerecord'    # object-relational mapping layer for Rails
require 'logger'              # writes log messages to a file or stream
require 'observer'            # for Observer pattern a.k.a Publish/Subscribe

# set up the load path
thisdir = File.dirname(__FILE__)
$:.unshift(thisdir)
$:.unshift(File.join(thisdir, "app", "models"))

# application requires
require 'call_state_event'
require 'cdr'
require 'configure'
require 'database_url'
require 'exceptions'
require 'party'


# :TODO: log the number of calls analyzed and how many succeeded vs. dups or
#        failures, also termination status
# :TODO: factor out the call resolver config into a separate class
# :TODO: Verify that AORs, contacts, and from/to tags are consistent.
#        The AORs and from tags must always be the same but contacts and to
#        tags can vary when a call forks. If there are inconsistencies then
#        discard the CSEs and log an error.

# The CallResolver analyzes call state events (CSEs) and computes call detail 
# records (CDRs).  It loads CSEs from a database and writes CDRs back into the
# same database.
class CallResolver
  include Observable    # so we can notify Call Resolver plugins of events

  # Constants
  
  # How many seconds are there in a day
  SECONDS_IN_A_DAY = 86400

  # Names of events that we send to plugins.
  EVENT_NEW_CDR = 'a new CDR has been created in the database'

public

  # Methods

  def initialize(config_file = nil)
    # Load the configuration from the config file.  If the config_file arg is
    # nil, then find the config file in the default location.
    load_config(config_file)

    @log_device = nil
    @log = nil
    init_logging
  end

  # Run daily processing, including purging and/or call resolution
  def daily_run(purge_flag = false, purge_time = 0)
    run_resolver = true
    do_purge = false

    # Only do a purge on a daily basis - not always
    do_purge = set_purge_enable_config(@config) 
    if set_daily_run_config(@config)
      get_daily_start_time(@config)
      
      # Get start time and end time
      start_time = @daily_start_time
      end_time = @daily_end_time
    else
      log.error("resolve: the --daily_flag is set, but the daily run is disabled in the configuration");
      run_resolver = false
    end
    
    # Resolve CDRs if enabled
    if run_resolver
      resolve(start_time, end_time)
    end
        
    # Purge if enabled
    if do_purge || purge_flag
      # Was a purge age explicitly set?
      if purge_time != 0
        purge_start_cdr = Time.now() - (SECONDS_IN_A_DAY * purge_time)
        purge_start_cse = purge_start_cdr         
        log.info("Purge override CSEs: #{purge_start_cse}, CDRs: #{purge_start_cdr}")
      else
        purge_start_cdr = get_purge_start_time_cdr(@config)
        purge_start_cse = get_purge_start_time_cse(@config)
        log.info("Normal purge CSEs: #{purge_start_cse}, CDRs: #{purge_start_cdr}")
      end  
      purge(purge_start_cse, purge_start_cdr)        
    end
  end

  # Resolve CSEs to CDRs
  def resolve(start_time, end_time, redo_flag = false)
    connect_to_cdr_database
    
    begin
      # Default the start time to 1 day before now
      start_time ||= Time.now() - SECONDS_IN_A_DAY
      
      # Default the end_time to 1 day after the start time
      end_time ||= start_time + SECONDS_IN_A_DAY
  
      start_run = Time.now
      log.info("resolve: Resolving calls from #{start_time.to_s} to " +
               "#{end_time.to_s}.  Running at #{start_run}.")

      # Load all CSEs in the time window, sorted by call ID.
      # :TODO: For performance/scalability (XPR-144) we can't just load all the
      # data at once.  Split the time window into subwindows and do one
      # subwindow at a time, noting incomplete CDRs and carrying those calls
      # forward into the next subwindow.
      events = load_events_in_time_window(start_time, end_time)
      
      # Resolve each call to yield 0-1 CDRs.  Save the CDRs.
      # The events array is subdivided into contiguous chunks with the same call
      # ID, each of which is the data for a single call.
      call_start = 0                          # index of first event in the call
      while call_start < events.length
        call_id = events[call_start].call_id  # call ID of current call
        
        # Look for the next event with a different call ID.  If we don't find
        # one, then the current call goes to the end of the events array.
        call_end = events.length - 1
        events[call_start + 1..-1].each_with_index do |event, index|
          if event.call_id != call_id
            call_end = index + call_start
            break
          end
        end
        
        # Resolve the call
        resolve_call(events[call_start..call_end])
        
        call_start = call_end + 1
      end

      end_run = Time.now
      log.info("resolve: Done at #{end_run}.  Analysis took #{end_run - start_run} seconds.")
  
    rescue
      # Backstop exception handler: don't let any exceptions propagate back
      # to the caller.  Log the error and the stack trace.  The log message has to
      # go on a single line, unfortunately.  Embed "\n" for syslogviewer.
      start_line = "\n        from "    # start each backtrace line with this
      log.error("Exiting because of error: \"#{$!}\"" + start_line +
                $!.backtrace.inject{|trace, line| trace + start_line + line})
    end
  end
  
  # Allow other components to use the Call Resolver log, since all logging should
  # go to a single shared file.  Make the configuration available.
  attr_reader :log, :log_device, :config
  
  attr_accessor :log_console, :log_level

private
  
  # Connect to the CDR database, if not already connected.  The CDR database
  # is the default database for all models.  Because there is only one CDR
  # database, the caller doesn't need to provide an URL.
  def connect_to_cdr_database
    unless ActiveRecord::Base.connected?
      ActiveRecord::Base.establish_connection(cdr_database_url.to_hash)
    end
  end

  def cdr_database_url
    # :TODO: read CDR database URL params from the Call Resolver config file
    # rather than just hardwiring default values.
    @cdr_database_url = DatabaseUrl.new unless @cdr_database_url
    @cdr_database_url
  end

  # Connect the CallStateEvent class to the CSE database at the specified URL.
  # With HA there are multiple CSE databases.
  def connect_to_cse_database(db_url)    
    log.debug("connect_to_cse_database: #{db_url}")
    CallStateEvent.establish_connection(db_url)
  end

  # Resolve the call, given an events array for that call, to yield 0-1 CDRs.
  # Persist the CDRs.  Do this as a single transaction.
  def resolve_call(events)
    call_id = events[0].call_id
    log.debug("Resolving a call: call ID = #{call_id} with #{events.length} events")
    
    # Load all events with this call_id, in ascending chronological order.
    # Don't constrain the query to a time window.  That allows us to handle
    # calls that span time windows.
    begin
      Cdr.transaction do
        # Find the first (earliest) call request event.
        call_req = find_first_call_request(events)
        if call_req
          # Read info from it and start constructing the CDR.
          cdr_data = start_cdr(call_req)
          
          # The forking proxy might ring multiple phones.  The dialog with each
          # phone is a separate call leg.
          # Pick the call leg with the best outcome and longest duration to be the
          # basis for the CDR.
          tags = best_call_leg(events)
          from_tag = tags[0]
          to_tag = tags[1]
  
          if to_tag                         # if there are any complete call legs
            # Fill the CDR from the call leg events.  The returned status is true
            # if that succeeded, false otherwise.
            status = finish_cdr(cdr_data, events, from_tag, to_tag)
          
            if status
              log.debug do
                cdr = cdr_data.cdr
                "Resolved a call from #{cdr_data.caller.aor} to " +
                "#{cdr_data.callee.aor} at #{cdr.start_time}, status = " +
                "#{cdr.termination_text}"
              end
              
              # Save the CDR and associated data
              save_cdr_data(cdr_data)
            end
          end
        end
      end
    # Don't let a single bad call blow up the whole run, if the exception was
    # raised by our code.  If it's an exception that we don't know about, then
    # bail out.
    rescue CallResolverException
      log.error("resolve_call: error with call ID = \"#{call_id}\", no CDR created: #{$!}") 
    end
  end

  # Load all events in the time window from HA distributed servers
  # Return a hash where the key is a call ID and values
  # are event arrays for that call ID, sorted by cseq.
  # :NOW: unit test this method
  def load_distrib_events_in_time_window(start_time, end_time)
    call_map = Hash.new
    
    # all_calls holds arrays, one array of event subarrays for each database.
    # Each subarray holds events for one call, sorted by cseq.
    all_calls = []
    
    cse_database_urls.each do |db_url|
      connect_to_cse_database(db_url)
      
      # Load events from this database
      events = load_events_in_time_window(start_time, end_time)
      log.debug("load_distrib_events_in_time_window: loaded #{events.length} " +
                "events from #{db_url}")
      
      # Divide the events into subarrays, one for each call.  Save the result.
      calls = split_events_by_call(events)
      all_calls << calls
    end
    
    # Put the event arrays in the hash table, merging on collisions
    merge_events_for_call(all_calls, call_map)

    call_map
  end
    
  # Put call event arrays in the hash table, merging on collisions. The
  # all_calls arg is an array of arrays, one for each CSE database. Each array
  # contains event arrays, where each event array is for a single call.
  def merge_events_for_call(all_calls, call_map)
    all_calls.each do |calls|
      calls.each do |call|
        call_id = call[0].call_id
        # If there is a hash entry already, then merge this partial call into
        # it, keeping the cseq sort. Otherwise create a new hash entry.
        # Typically each call comes completely from one server, but that is not
        # always true.
        entry = call_map[call_id]
        if entry
          # merge the events together and re-sort by cseq
          entry += call
          entry.sort!{|x, y| x.cseq <=> y.cseq}
          
          # not sure why I have to do this -- entry has been modified in place,
          # yes? -- but otherwise the hash entry doesn't get updated correctly
          call_map[call_id] = entry   
        else
          # create a new hash entry
          call_map[call_id] = call
        end
      end
    end
  end

  # Given an events array where the events for a given call are contiguous,
  # split the array into subarrays, one for each call.  Return the array of
  # subarrays.
  def split_events_by_call(events)
    calls = []
    call_start = 0                          # index of first event in the call
    while call_start < events.length
      call_id = events[call_start].call_id  # call ID of current call
      
      # Look for the next event with a different call ID.  If we don't find
      # one, then the current call goes to the end of the events array.
      call_end = events.length - 1
      events[call_start + 1..-1].each_with_index do |event, index|
        if event.call_id != call_id
          call_end = index + call_start
          break
        end
      end
  
      # Add the subarray to the calls array
      calls << events[call_start..call_end]
  
      # On to the next call
      call_start = call_end + 1
    end
    calls
  end

  # Load all events in the time window, sorted by call ID then by cseq.
  # Return the events in an array.
  def load_events_in_time_window(start_time, end_time)
    events =
      CallStateEvent.find(
        :all,
        :conditions => "event_time >= '#{start_time}' and event_time < '#{end_time}'",
        :order => "call_id, cseq")
    log.debug("load_events: loaded #{events.length} events between #{start_time} and #{end_time}")
    events
  end
   
  # Load all events with the given call_id, in ascending chronological order.
  def load_events_with_call_id(call_id)
    events =
      CallStateEvent.find(
        :all,
        :conditions => ["call_id = :call_id", {:call_id => call_id}],
        :order => "event_time")
    log.debug("load_events: loaded #{events.length} events with call_id = #{call_id}")
    return events
  end
  
  # Load the call IDs for the specified time window.
  def load_call_ids(start_time, end_time)
    # This query returns an array of objects of class CallStateEvent, but they
    # only have the call_id attribute defined.  Convert to an array of strings
    # and return that.
    results = CallStateEvent.find_by_sql(
      "select distinct call_id from call_state_events " +
      "where event_time >= '#{start_time}' and event_time <= '#{end_time}'")
    results.collect do |obj|
      obj.call_id
    end
  end  
  
  # Find and return the first (earliest) call request event.
  # Return nil if there is no such event.
  def find_first_call_request(events)
    event = events.find {|event| event.call_request?}

    log.debug do
      message = event ? "found #{event}" : "no call request found"
      'find_first_call_request: ' + message
    end
    
    return event
  end

  # Read info from the call request event and start constructing the CDR.
  # Return the new CDR.
  def start_cdr(call_req)
    caller = Party.new(:aor => call_req.caller_aor,
                       :contact => Utils.contact_without_params(call_req.contact))
    callee = Party.new(:aor => call_req.callee_aor)
    cdr = Cdr.new(:call_id => call_req.call_id,
                  :from_tag => call_req.from_tag,
                  :start_time => call_req.event_time,
                  :termination => Cdr::CALL_REQUESTED_TERM)
    CdrData.new(cdr, caller, callee)
  end

  # Pick the call leg with the best outcome and longest duration to be the
  # basis for the CDR.  Return the to_tag for that call leg.  A call leg is a
  # set of events with a common to_tag.
  # Return nil if there is no such call leg.
  def best_call_leg(events)     # array of events with a given call ID
    to_tag = nil                # result: the to_tag for the best call leg
    from_tag = nil
    
    # If there are no call_end events, then the call failed
    call_failed = !events.any? {|event| event.call_end?}
    
    # Find the call leg with the best outcome and longest duration.
    # If the call succeeded, then look for the call end event with the biggest
    # timestamp.  Otherwise look for the call failure event with the biggest
    # timestamp.  Events have already been sorted for us in timestamp order.
    final_event_type = call_failed ?
                       CallStateEvent::CALL_FAILURE_TYPE :
                       CallStateEvent::CALL_END_TYPE
    events.reverse_each do |event|
      if event.event_type == final_event_type
        to_tag = event.to_tag
        from_tag = event.from_tag
        break
      end
    end
    
    if !to_tag
      # If there is no final event, then try to at least find a call_setup event
      events.reverse_each do |event|
        if event.call_setup?
          to_tag = event.to_tag
          from_tag = event.from_tag
          break
        end
      end
    end

    [ from_tag, to_tag ]
  end
  
  # Fill in the CDR from a call leg consisting of the events with the given
  # to_tag.  Return true if successful, false otherwise.
  def finish_cdr(cdr_data, events, from_tag, to_tag)
    status = false                # return value: did we fill in the CDR?
    cdr = cdr_data.cdr      

    # Get the events for the call leg
    call_leg = events.find_all {|event| event.to_tag == to_tag or event.from_tag == to_tag}

    # Find the call_setup event
    call_setup = call_leg.find {|event| event.call_setup?}

    if call_setup
      # The call was set up, so mark it provisionally as in progress.
      cdr.termination = Cdr::CALL_IN_PROGRESS_TERM
 
      # We have enough data now to build the CDR.
      status = true
      
      # Get data from the call_setup event
      cdr_data.callee.contact = Utils.contact_without_params(call_setup.contact)
      cdr.to_tag = call_setup.to_tag
      cdr.connect_time = call_setup.event_time
      
      # Get data from the call_end or call_failure event
      call_end = call_leg.find {|event| event.call_end?}
      if call_end
        cdr.termination = Cdr::CALL_COMPLETED_TERM    # successful completion
        cdr.end_time = call_end.event_time
      else
        # Couldn't find a call_end event, try for call_failure
        status = handle_call_failure(call_leg, cdr)
      end
    else
      # No call_setup event, so look for a call_failure event
      status = handle_call_failure(call_leg, cdr)
    end
    
    status
  end

  # Look for a call_failure event in the call_leg events array.  If we find one,
  # then fill in CDR info and return success.  Otherwise return failure.
  def handle_call_failure(call_leg, cdr)
    status = false      
    call_failure = call_leg.find {|event| event.call_failure?}
    if call_failure
      # found a call_failure event, use it
      # :TODO: consider optimizing space usage by not setting the
      # failure_reason if it is the default value for the failure_status
      # code. For example, the 486 error has the default reason "Busy Here".      
      cdr.to_tag = call_failure.to_tag  # may already be filled in from call_setup
      cdr.termination = Cdr::CALL_FAILED_TERM
      cdr.end_time = call_failure.event_time
      cdr.failure_status = call_failure.failure_status
      cdr.failure_reason = call_failure.failure_reason
      status = true
    end
    status
  end

  # Save the CDR and associated data.
  # Be sure not to duplicate existing data.  For example, the caller
  # may already be present in the parties table from a previous call,
  # or a complete CDR may have been created for this call in a
  # previous run.
  # Raise a CallResolverException if the save fails for some reason.
  # :TODO: support the redo flag for recomputing CDRs
  def save_cdr_data(cdr_data)
    # define variables for cdr_data components
    cdr = cdr_data.cdr
    caller = cdr_data.caller
    callee = cdr_data.callee
    
    # Continue only if a complete CDR doesn't already exist
    db_cdr = find_cdr_by_dialog(cdr.call_id, cdr.from_tag, cdr.to_tag)
    
    # If we found an existing CDR, then log that for debugging
    if db_cdr
      log.debug do
        "save_cdr_data: found an existing CDR.  Call ID = #{db_cdr.call_id}, " +
        "termination = #{db_cdr.termination}, ID = #{db_cdr.id}."
      end
    end
    
    # Save the CDR as long as there is no existing, complete CDR for that call ID.
    if (!db_cdr or !db_cdr.complete?)
      # Save the caller and callee if they don't already exist
      caller = save_party_if_new(caller)
      callee = save_party_if_new(callee)
      cdr.caller_id = caller.id
      cdr.callee_id = callee.id

      # Call the Observable method indicating a state change.
      changed
  
      # Notify plugins of the new CDR.
      # "notify_observers" is a method of the Observable module, which is mixed
      # in to the CallResolver.
      notify_observers(EVENT_NEW_CDR,       # event type
                       cdr_data)            # the new CDR
      
      # Save the CDR.  If there is an incomplete CDR already, then replace it.
      save_cdr(cdr)
    end
  end
  
  # Given the parameters identifying a SIP dialog -- call_id, from_tag, and
  # to_tag -- return the CDR, or nil if a CDR could not be found.
  def find_cdr_by_dialog(call_id, from_tag, to_tag)
    Cdr.find(
      :first,
      :conditions =>
        ["call_id = :call_id and from_tag = :from_tag and to_tag = :to_tag",
         {:call_id => call_id, :from_tag => from_tag, :to_tag => to_tag}])
  end
  
  # Given an in-memory CDR, find that CDR in the database and return it.
  # If the CDR is not in the database, then return nil.
  def find_cdr(cdr)
    if !cdr.call_id or !cdr.from_tag or !cdr.to_tag
      raise(ArgumentError, "find_cdr: call_id, from_tag, or to_tag is nil")
    end
    find_cdr_by_dialog(cdr.call_id, cdr.from_tag, cdr.to_tag)
  end
   
  # Given an in-memory Party, find that Party in the database and return it.
  # If the Party is not in the database, then return nil.
  def find_party(party)
    Party.find_by_aor_and_contact(party.aor, party.contact)
  end
  
  # Save the input CDR.
  # If there is an incomplete CDR already in the DB, then replace it.
  # If there is a complete CDR already in the DB, then replace it only if the
  # replace input is true.
  # Return the saved CDR.
  #
  # :LATER: Handle the race condition where after we check for the existence
  # of the CDR and before we save it, another thread saves a CDR with the same
  # dialog ID.  See save_party_if_new for inspiration.  CDRs are different
  # because we have to replace the CDR that's in the database.  But we don't
  # need to deal with this issue now because there is only one thread creating
  # CDRs.
  def save_cdr(cdr, replace = false)
    do_save = true
    
    # Find any existing CDR with the same SIP dialog ID
    cdr_in_db = find_cdr(cdr)
    
    # If there is an existing CDR, then decide whether to replace it 
    if cdr_in_db
      if !cdr_in_db.complete? or
         (cdr_in_db.complete? and replace)
        # Either the CDR is incomplete, or it is complete but we have been told
        # to replace it
        cdr_in_db.destroy
        do_save = true
      else
        # Don't replace the existing CDR
        do_save = false
      end
    end  
    
    if do_save
      # Save the CDR.  Call "save!" rather than "save" so that we'll get an
      # exception if the save fails.
      if cdr.save!
        cdr_in_db = cdr
      else
        raise(CallResolverException, 'save_cdr: save failed', caller)
      end
    end
      
    cdr_in_db
  end
  
  # Save the input Party if it is not already in the database.
  # Return the Party, either the saved input Party or the Party with equal
  # values that was already in the database.
  def save_party_if_new(party)
    party_in_db = find_party(party)
    if !party_in_db
      begin
        # Save the party.  Call "save!" rather than "save" so that we'll get an
        # exception if the save fails.          
        if party.save!
          party_in_db = party
        else
          raise(CallResolverException, 'save_party_if_new: save failed', caller)
        end
      rescue ActiveRecord::StatementInvalid
        # Because we are doing optimistic locking, there is a small chance that
        # a Party got saved just after we did the check above, resulting in a
        # DB integrity violation when we try to save a duplicate.  In this case
        # return the Party that is in the database.
        log.debug("save_party_if_new: unusual race condition detected")
        party_in_db = find_party(party)
        
        # The Party had better be in the database now.  If not then rethrow,
        # we are lost in the woods without a flashlight.
        if !party_in_db
          log.error("save_party_if_new: party should be in database but isn't")
          raise
        end
      end
    end
    
    party_in_db
  end
  
  # Purge records from call state event and cdr tables, making sure
  # to delete unreferenced entries in the parties table.
  def purge(start_time_cse, start_time_cdr)
    connect_to_cdr_database
    
    # Purge CSEs
    CallStateEvent.transaction do
      log.debug("purge: purging CSEs where event_time <= #{start_time_cse}")    
      CallStateEvent.delete_all(["event_time <= '#{start_time_cse}'"])
    end
    
    # Purge CDRs
    Cdr.transaction do
      log.debug("purge: purging CDRs where event_time <= #{start_time_cdr}")      
      Cdr.delete_all(["end_time <= '#{start_time_cdr}'"])
      
      # Clean up the parties table, removing entries that are no longer referenced by CDRs
      Party.delete_all(["not exists (select * from cdrs where caller_id = parties.id or callee_id = parties.id)"])
    end    
  
    # Garbage-collect deleted records and tune performance.
    # See http://www.postgresql.org/docs/8.0/static/sql-vacuum.html .
    # :TODO: For HA, purge multiple databases, both CSE and CDR
    # Must be done outside of a transaction or we'll get an error.
    ActiveRecord::Base.connection.execute("VACUUM ANALYZE")
  end
  
  #-----------------------------------------------------------------------------
  # Configuration
  
  # Default config file path
  DEFAULT_CONFIG_FILE = '/etc/sipxpbx/callresolver-config'
  
  # If set, then this becomes a prefix to the default config file path
  SIPX_PREFIX = 'SIPX_PREFIX'

  # If the daily run is enabled, then it happens at 4 AM, always
  DAILY_RUN_TIME = '04:00'

  # Configuration parameters and defaults

  # Whether console logging is enabled or disabled.  Legal values are "ENABLE"
  # or "DISABLE".  Comparison is case-insensitive with this and other values.
  LOG_CONSOLE_CONFIG = 'SIP_CALLRESOLVER_LOG_CONSOLE'
  LOG_CONSOLE_CONFIG_DEFAULT = Configure::DISABLE
  
  # The directory holding log files.  The default value is prefixed by
  # $SIPX_PREFIX if that environment variable is defined.
  LOG_DIR_CONFIG = 'SIP_CALLRESOLVER_LOG_DIR'
  LOG_DIR_CONFIG_DEFAULT = '/var/log/sipxpbx'
  
  # Logging severity level
  LOG_LEVEL_CONFIG = 'SIP_CALLRESOLVER_LOG_LEVEL'
  LOG_LEVEL_CONFIG_DEFAULT = 'NOTICE'
  
  LOG_FILE_NAME = 'sipcallresolver.log'

  DAILY_RUN = 'SIP_CALLRESOLVER_DAILY_RUN'
  DAILY_RUN_DEFAULT = Configure::DISABLE
  
  PURGE_ENABLE = 'SIP_CALLRESOLVER_PURGE'
  PURGE_ENABLE_DEFAULT = Configure::ENABLE
  
  PURGE_AGE_CDR = 'SIP_CALLRESOLVER_PURGE_AGE_CDR'
  PURGE_AGE_CDR_DEFAULT = '35'
  
  PURGE_AGE_CSE = 'SIP_CALLRESOLVER_PURGE_AGE_CSE'
  PURGE_AGE_CSE_DEFAULT = '7'
  
  
  # Map from the name of a log level to a Logger level value.
  # Map the names of sipX log levels (DEBUG, INFO, NOTICE, WARNING, ERR, CRIT,
  # ALERT, EMERG) and Logger log levels (DEBUG, INFO, WARN, ERROR, FATAL) into
  # Logger log levels.
  LOG_LEVEL_MAP = {
    "DEBUG"   => Logger::DEBUG, 
    "INFO"    => Logger::INFO, 
    "NOTICE"  => Logger::INFO, 
    "WARN"    => Logger::WARN,
    "WARNING" => Logger::WARN,
    "ERR"     => Logger::ERROR, 
    "CRIT"    => Logger::FATAL,
    "ALERT"   => Logger::FATAL,
    "EMERG"   => Logger::FATAL
  }
  
  # Load the configuration from the config file.  If the config_file arg is
  # nil, then find the config file in the default location.
  def load_config(config_file)
    # If the config_file is nil, then apply defaults
    config_file = apply_config_file_default(config_file)
    
    if File.exists?(config_file)
      # Load the config file
      @config = Configure.new(config_file)
    else
      @config = {}
    end

    # Read config params, applying defaults
    set_log_console_config(@config)
    set_log_dir_config(@config)
    set_log_level_config(@config)
  end
  
  # Set the console logging from the configuration.
  # Return the console logging boolean.
  def set_log_console_config(config)
    # Look up the config param
    @log_console = config[LOG_CONSOLE_CONFIG]
    
    # Apply the default if the param was not specified
    @log_console ||= LOG_CONSOLE_CONFIG_DEFAULT

    # Convert to a boolean
    if @log_console.casecmp(Configure::ENABLE) == 0
      @log_console = true
    elsif @log_console.casecmp(Configure::DISABLE) == 0
      @log_console = false
    else
      raise(ConfigException, "Unrecognized value \"#{@log_console}\" for " +
            "#{LOG_CONSOLE_CONFIG}.  Must be ENABLE or DISABLE.")
    end
  end
  
  # Set the log directory from the configuration.  Return the log directory.
  def set_log_dir_config(config)
    # Look up the config param
    @log_dir = config[LOG_DIR_CONFIG]
    
    # Apply the default if the param was not specified
    if !@log_dir
      @log_dir = LOG_DIR_CONFIG_DEFAULT
      
      # Prepend the prefix dir if $SIPX_PREFIX is defined
      prefix = ENV[SIPX_PREFIX]
      if prefix
        @log_dir = File.join(prefix, @log_dir)
      end      
    end
    
    @log_dir
  end
  
  # Set the log level from the configuration.  Return the log level.
  def set_log_level_config(config)
    # Look up the config param
    log_level_name = config[LOG_LEVEL_CONFIG]
    
    # Apply the default if the param was not specified
    log_level_name ||= LOG_LEVEL_CONFIG_DEFAULT
    
    # Convert the log level name to a Logger log level
    @log_level = log_level_from_name(log_level_name)
    
    # If we don't recognize the name, then refuse to run.  Would be nice to
    # log a warning and continue, but there is no log yet!
    if !@log_level
      raise(CallResolverException, "Unknown log level: #{log_level_name}")   
    end
    
    @log_level
  end

  # Given the name of a log level, return the log level value, or nil if the
  # name is not recognized.
  # We accept sipX log levels and Logger log levels and map them into Logger log
  # levels.
  def log_level_from_name(name)
    LOG_LEVEL_MAP[name]
  end
  
  # Given a config_file name, if it is non-nil then just return it.
  # If it's nil then return the default config file path, prepending
  # $SIPX_PREFIX if that has been set.
  def apply_config_file_default(config_file)
    if !config_file
      config_file = DEFAULT_CONFIG_FILE
      prefix = ENV[SIPX_PREFIX]
      if prefix
        config_file = File.join(prefix, config_file)
      end
    end
    
    config_file
  end
  
  # Set up logging.  Return the Logger.
  def init_logging
    # If console logging was specified, then do that.  Otherwise log to a file.
    if @log_console
      @log_device = STDOUT
    else
      if File.exists?(@log_dir)
        log_file = File.join(@log_dir, LOG_FILE_NAME)
        @log_device = log_file
        # If the file exists, then it must be writable. If it doesn't exist,
        # then the directory must be writable.
        if File.exists?(log_file)
          if !File.writable?(log_file)
            puts("init_logging: Log file \"#{log_file}\" exists but is not writable. " +
                 "Log messages will go to the console.")
            @log_device = STDOUT
          end
        else
          if !File.writable?(@log_dir)
            puts("init_logging: Log directory \"#{@log_dir}\" is not writable. " +
                 "Log messages will go to the console.")
            @log_device = STDOUT
          end
        end
      else
        puts("Unable to open log file, log directory \"#{@log_dir}\" does not " +
             "exist.  Log messages will go to the console.")
        @log_device = STDOUT
      end
    end
    @log = Logger.new(@log_device)

    # Set the log level from the configuration
    @log.level = @log_level

    # Override the log level to DEBUG if $DEBUG is set.
    # :TODO: figure out why this isn't working.
    if $DEBUG then
      @log.level = Logger::DEBUG
    end
    
    @log
  end

  # Enable/disable the daily run from the configuration.
  # Return true if daily runs are enabled, false otherwise.
  def set_daily_run_config(config)
    # Look up the config param
    @daily_run = config[DAILY_RUN]
    
    # Apply the default if the param was not specified
    @daily_run ||= DAILY_RUN_DEFAULT

    # Convert to a boolean
    if @daily_run.casecmp(Configure::ENABLE) == 0
      @daily_run = true
    elsif @daily_run.casecmp(Configure::DISABLE) == 0
      @daily_run = false
    else
      raise(ConfigException, "Unrecognized value \"#{@daily_run}\" for " +
            "#{DAILY_RUN}.  Must be ENABLE or DISABLE.")
    end
  end

  # Compute the start time of the daily call resolver run.
  # We decided not to make this configurable.  Too complicated given that the
  # cron job always runs at a fixed time.
  def get_daily_start_time(config)
    # Always start the time window at the time the resolver runs
    daily_start = DAILY_RUN_TIME
    
    # Turn the start time into a date/time.
    # Get today's date, cut out the date and paste our start time into
    # a time string.
    today = Time.now
    todayString = today.strftime("%m/%d/%YT")
    startString = todayString + daily_start
    
    # Convert to time, start same time yesterday
    @daily_start_time = Time.parse(startString)
    #log.debug("get_daily_start_time: String #{startString}, time #{@daily_start_time}")    
    @daily_end_time = @daily_start_time
    @daily_start_time -= SECONDS_IN_A_DAY   # 24 hours
  end
  
  # Enable/disable the daily run from the configuration.
  # Return true if purging is enabled, false otherwise.
  def set_purge_enable_config(config)
    # Look up the config param
    @purge_enable = config[PURGE_ENABLE]
    
    # Apply the default if the param was not specified
    @purge_enable ||= PURGE_ENABLE_DEFAULT

    # Convert to a boolean
    if @purge_enable.casecmp(Configure::ENABLE) == 0
      @purge_enable = true
    elsif @purge_enable.casecmp(Configure::DISABLE) == 0
      @purge_enable = false
    else
      raise(ConfigException, "Unrecognized value \"#{@purge_enable}\" for " +
            "#{PURGE_ENABLE}.  Must be ENABLE or DISABLE.")
    end
  end  
  
  # Compute start time of CDR records to be purged from configuration
  def get_purge_start_time_cdr(config)
    # Look up the config param
    purge_age = config[PURGE_AGE_CDR]
    
    # Apply the default if the param was not specified
    purge_age ||= PURGE_AGE_CDR_DEFAULT
    
    # Convert to number
    purge_age = purge_age.to_i
    
    if (purge_age <= 0)
      raise(ConfigException, "Illegal value \"#{@purge_age}\" for " +
            "#{PURGE_AGE_CDR}.  Must be a number greater than 0.")
    end
    # Get today's date
    today = Time.now
    @purge_start_time_cdr = today - (SECONDS_IN_A_DAY * purge_age)
  end    
    
  # Compute start time of CSE records to be purged from configuration
  def get_purge_start_time_cse(config)
    # Look up the config param
    purge_age = config[PURGE_AGE_CSE]
    
    # Apply the default if the param was not specified
    purge_age ||= PURGE_AGE_CSE_DEFAULT
    
    # Convert to number
    purge_age = purge_age.to_i
    
    if (purge_age <= 0)
      raise(ConfigException, "Illegal value \"#{@purge_age}\" for " +
            "#{PURGE_AGE_CSE}.  Must be a number greater than 0.")
    end    
                
    # Get today's date
    today = Time.now
    @purge_start_time_cse = today - (SECONDS_IN_A_DAY * purge_age)
  end
  
  # Return an array of CSE database URLs.  With an HA configuration, there are
  # multiple CSE databases.  Note that usually one of these URLs is identical
  # to the CDR database URL, since a standard master server runs both the
  # proxies and the call resolver, which share the SIPXCDR database.
  # :TODO: Base these URLs on the config rather than just hardwiring the single
  # SIPXCDR URL (XPB-562).
  def cse_database_urls
    unless @cse_database_urls
      @cse_database_urls = [cdr_database_url]
    end
    
    @cse_database_urls
  end
  
  # For testing purposes, make it possible to override the CSE database URLs
  def cse_database_urls=(urls)
    @cse_database_urls = urls
  end
  
  #-----------------------------------------------------------------------------

end    # class CallResolver


# CdrData holds data for a CDR that is under construction: the CDR and the
# associated caller and callee parties.  Because this data has not yet been
# written to the DB, we can't use the foreign key relationships that usually
# link the CDR to the caller and callee.
class CdrData
  attr_accessor :cdr, :caller, :callee

  def initialize(cdr = Cdr.new, caller = Party.new, callee = Party.new)
    @cdr = cdr
    @caller = caller
    @callee = callee
  end
end
