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
package org.sipfoundry.sipxconfig.site.acd;

import org.apache.tapestry.IPage;
import org.apache.tapestry.IRequestCycle;
import org.apache.tapestry.callback.PageCallback;
import org.apache.tapestry.event.PageBeginRenderListener;
import org.apache.tapestry.event.PageEvent;
import org.sipfoundry.sipxconfig.acd.AcdAgent;
import org.sipfoundry.sipxconfig.acd.AcdContext;
import org.sipfoundry.sipxconfig.components.PageWithCallback;
import org.sipfoundry.sipxconfig.components.TapestryUtils;

public abstract class EditAcdAgent extends PageWithCallback implements PageBeginRenderListener {
    public static final String PAGE = "EditAcdAgent";

    public abstract AcdContext getAcdContext();

    public abstract AcdAgent getAcdAgent();

    public abstract void setAcdAgent(AcdAgent acdAgent);

    public abstract Integer getAcdAgentId();

    public abstract void setAcdAgentId(Integer acdAgentId);

    public void pageBeginRender(PageEvent event_) {
        AcdAgent acdAgent = getAcdAgent();
        if (acdAgent != null) {
            return;
        }
        AcdContext acdContext = getAcdContext();
        Integer id = getAcdAgentId();
        acdAgent = acdContext.loadAgent(id);
        setAcdAgent(acdAgent);
    }

    public void formSubmit() {
        if (!TapestryUtils.isValid(this)) {
            return;
        }
        // make sure that queue is fetched from DB on render
        setAcdAgent(null);
    }

    public void apply() {
        if (TapestryUtils.isValid(this)) {
            AcdContext acdContext = getAcdContext();
            AcdAgent acdAgent = getAcdAgent();
            acdContext.store(acdAgent);
        }
    }

    public IPage editQueue(IRequestCycle cycle, Integer id) {
        EditAcdQueue editPage = (EditAcdQueue) cycle.getPage(EditAcdQueue.PAGE);
        editPage.setAcdQueueId(id);
        editPage.setCallback(new PageCallback(this));
        return editPage;
    }
}