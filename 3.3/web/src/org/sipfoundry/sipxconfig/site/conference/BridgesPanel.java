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
package org.sipfoundry.sipxconfig.site.conference;

import java.util.Collection;

import org.sipfoundry.sipxconfig.components.TablePanel;
import org.sipfoundry.sipxconfig.conference.Bridge;
import org.sipfoundry.sipxconfig.conference.ConferenceBridgeContext;

public abstract class BridgesPanel extends TablePanel {
    public BridgesPanel() {
        super(Bridge.class);
        // TODO Auto-generated constructor stub
    }

    public abstract ConferenceBridgeContext getConferenceBridgeContext();

    protected void removeRows(Collection selectedRows) {
        getConferenceBridgeContext().removeBridges(selectedRows);
    }
}
