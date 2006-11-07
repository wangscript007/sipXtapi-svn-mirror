#
# Copyright (C) 2006 SIPfoundry Inc.
# Licensed by SIPfoundry under the LGPL license.
# 
# Copyright (C) 2006 Pingtel Corp.
# Licensed to SIPfoundry under a Contributor Agreement.
#
##############################################################################

# The Configure class reads in config params from a standard sipX config file
# and creates a map of param-values that is publicly available via the "[]"
# operator.  This class is generic, it is not specifically for Call Resolver.

require 'utils/exceptions'


class Configure
  
  # Config file syntax
  COMMENT_CHAR = '#'
  SEPARATOR_CHAR = ':'    # separate param names from values
  
  # String constants
  DISABLE = 'DISABLE'
  ENABLE = 'ENABLE'
  
  # Initialize inputs:
  #   config_file - relative or absolute path to config file (String)
  def initialize(map = nil)
    # create a Hash to hold the config param name to value map
    @map = map || Hash.new
  end
  
  # Return the value for the named parameter, or nil if no such parameter exists
  def [](param_name)
    return @map[param_name]
  end
  
  
  def enabled?(param_name, default = nil)
    value = @map.fetch(param_name, default)
    raise ConfigException, "No value for #{param_name}" unless value
    case value.upcase
    when ENABLE: true
    when DISABLE: false
    else raise ConfigException, %Q/Unrecognized value "#{value}" for "#{param_name}"./
    end
  end
  
  private
  
  # Set param values, mainly for testing.
  def []=(param_name, param_value)
    @map[param_name] = param_value
  end
  
  
  class << self
    def from_file(config_file)
      map = parse_config(config_file)
      Configure.new(map)
    end
    
    # Parse the config file and construct a hash mapping param names to values.
    def parse_config(config_file)
      map = {}
      File.open(config_file, 'r') do |file|
        line_num = 0
        file.each_line do |line|
          line_num += 1
          name, value = parse_line(line, line_num)
          next unless name
          map[name] = value unless value.empty?
        end
      end
      return map
    end
    
    def parse_line(line, line_num = 0)
      # discard any trailing comments
      line = line.split(COMMENT_CHAR, 2)[0].strip
      return if line.empty?
      
      # split the line into name and value
      name, value = line.split(SEPARATOR_CHAR, 2)
      
      if !value
        msg = "Missing \"#{SEPARATOR_CHAR}\" on line #{line_num}: #{line}"
        raise(ConfigException, msg)
      end
      
      return name.strip, value.strip
    end  
  end
  
end
