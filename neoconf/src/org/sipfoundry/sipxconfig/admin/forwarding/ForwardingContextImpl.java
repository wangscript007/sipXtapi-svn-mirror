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
package org.sipfoundry.sipxconfig.admin.forwarding;

import org.sipfoundry.sipxconfig.phone.User;
import org.springframework.orm.hibernate.HibernateTemplate;
import org.springframework.orm.hibernate.support.HibernateDaoSupport;

/**
 * ForwardingContextImpl
 * 
 * TODO: for testing purposes returns a single call sequence (for all users and for all ids)
 */
public class ForwardingContextImpl extends HibernateDaoSupport implements ForwardingContext {
    /**
     * Looks for a call sequence associated with a given user.
     * 
     * This version just assumes that CallSequence id is the same as user id. 
     * More general implementation would run a query.
     * <code>
     *      String ringsForUser = "from CallSequence cs where cs.user = :user";
     *      hibernate.findByNamedParam(ringsForUser, "user", user);
     * </code>
     * 
     * @param user for which CallSequence object is retrieved
     */
    public CallSequence getCallSequenceForUser(User user) {
        HibernateTemplate hibernate = getHibernateTemplate();
        return (CallSequence) hibernate.load(CallSequence.class, new Integer(user.getId()));
    }

    public void saveCallSequence(CallSequence callSequence) {
        getHibernateTemplate().update(callSequence);
    }

    public void flush() {
        getHibernateTemplate().flush();
    }
}
