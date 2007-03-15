/*
 * 
 * 
 * Copyright (C) 2005 SIPfoundry Inc.
 * Licensed by SIPfoundry under the LGPL license.
 * 
 * Copyright (C) 2005 Pingtel Corp.
 * Licensed to SIPfoundry under a Contributor Agreement.
 * 
 * $
 */
package org.sipfoundry.sipxconfig.admin.callgroup;

import java.util.Collection;
import java.util.List;

import org.sipfoundry.sipxconfig.admin.commserver.AliasProvider;
import org.sipfoundry.sipxconfig.alias.AliasOwner;

public interface CallGroupContext extends AliasOwner, AliasProvider {
    public static final String CONTEXT_BEAN_NAME = "callGroupContext";

    void activateCallGroups();

    CallGroup loadCallGroup(Integer id);

    List getCallGroups();

    void storeCallGroup(CallGroup callGroup);

    void removeCallGroups(Collection ids);

    void duplicateCallGroups(Collection ids);

    void removeUser(Integer userId);

    void addUsersToCallGroup(Integer callGroupId, Collection ids);

    void clear();
}