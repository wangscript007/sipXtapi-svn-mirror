/*
 * 
 * 
 * Copyright (C) 2006 SIPfoundry Inc.
 * Licensed by SIPfoundry under the LGPL license.
 * 
 * Copyright (C) 2006 Pingtel Corp.
 * Licensed to SIPfoundry under a Contributor Agreement.
 * 
 * $
 */
package org.sipfoundry.sipxconfig.domain;

import java.util.Collection;
import java.util.Collections;
import java.util.List;

import org.sipfoundry.sipxconfig.admin.commserver.SipxServer;
import org.sipfoundry.sipxconfig.admin.dialplan.DialingRule;
import org.sipfoundry.sipxconfig.common.DaoUtils;
import org.sipfoundry.sipxconfig.common.SipxHibernateDaoSupport;

public class DomainManagerImpl extends SipxHibernateDaoSupport<Domain> implements DomainManager {
    
    // do not reference m_server, see note in spring file
    private SipxServer m_server;

    public SipxServer getServer() {
        return m_server;
    }

    public void setServer(SipxServer server) {
        m_server = server;
    }

    /**
     * @return non-null unless test environment
     */
    public Domain getDomain() {
        Domain domain = getExistingDomain();
        if (domain == null) {
            throw new DomainNotInitializedException();
        }

        return domain;
    }
    
    public void saveDomain(Domain domain) {
        if (domain.isNew()) {
            Domain existing = getExistingDomain();
            if (existing != null) {
                getHibernateTemplate().delete(getDomain());
            }
        }
        getHibernateTemplate().saveOrUpdate(domain);
        
        getServer().setDomainName(domain.getName());
        getServer().setRegistrarDomainAliases(domain.getAliases());
        getServer().applySettings();
    }
    
    private Domain getExistingDomain() {
        Collection domains = getHibernateTemplate().findByNamedQuery("domain");
        Domain domain = (Domain) DaoUtils.requireOneOrZero(domains, "named query: domain");
        return domain;        
    }

    public List<DialingRule> getDialingRules() {
        List<DialingRule> rules;
        Domain d = getDomain();
        if (d.hasAliases()) {
            DialingRule domainRule = new DomainDialingRule(getDomain());
            rules = Collections.singletonList(domainRule);
        } else {
            rules = Collections.EMPTY_LIST;
        }
        return rules;
    }  
}