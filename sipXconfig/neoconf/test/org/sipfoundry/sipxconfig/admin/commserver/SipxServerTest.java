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
package org.sipfoundry.sipxconfig.admin.commserver;

import java.io.InputStream;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;

import junit.framework.TestCase;

import org.easymock.EasyMock;
import org.easymock.IMocksControl;
import org.sipfoundry.sipxconfig.TestHelper;
import org.sipfoundry.sipxconfig.admin.forwarding.AliasMapping;
import org.sipfoundry.sipxconfig.common.CoreContext;
import org.sipfoundry.sipxconfig.setting.Setting;

public class SipxServerTest extends TestCase {

    private SipxServer m_server;

    protected void setUp() throws Exception {
        m_server = new SipxServer();
        m_server.setConfigDirectory(TestHelper.getTestDirectory());
        InputStream configDefs = getClass().getResourceAsStream("config.defs");
        TestHelper
                .copyStreamToDirectory(configDefs, TestHelper.getTestDirectory(), "config.defs");
        InputStream sipxpresence = getClass().getResourceAsStream("sipxpresence-config.test.in");
        TestHelper.copyStreamToDirectory(sipxpresence, TestHelper.getTestDirectory(),
                "sipxpresence-config.in");
        InputStream registrar = getClass().getResourceAsStream("registrar-config.test.in");
        TestHelper.copyStreamToDirectory(registrar, TestHelper.getTestDirectory(),
                "registrar-config");
        // we read server location from sipxpresence-config
        sipxpresence = getClass().getResourceAsStream("sipxpresence-config.test.in");
        TestHelper.copyStreamToDirectory(sipxpresence, TestHelper.getTestDirectory(),
                "sipxpresence-config");
        m_server.setModelFilesContext(TestHelper.getModelFilesContext());
        registrar = getClass().getResourceAsStream("registrar-config.test.in");
        TestHelper.copyStreamToDirectory(registrar, TestHelper.getTestDirectory(),
                "registrar-config.in");
    }

    public void testGetSetting() {
        Setting settings = m_server.getSettings();
        assertNotNull(settings);
    }

    public void testGetAliasMappings() {
        IMocksControl coreContextCtrl = EasyMock.createControl();
        CoreContext coreContext = coreContextCtrl.createMock(CoreContext.class);
        coreContext.getDomainName();
        coreContextCtrl.andReturn("domain.com").atLeastOnce();
        coreContextCtrl.replay();

        m_server.setCoreContext(coreContext);
        assertNotNull(m_server.getPresenceServerUri());

        Collection aliasMappings = m_server.getAliasMappings();

        assertEquals(2, aliasMappings.size());
        for (Iterator i = aliasMappings.iterator(); i.hasNext();) {
            AliasMapping am = (AliasMapping) i.next();
            assertTrue(am.getIdentity().matches("\\*8\\d@domain.com"));
            assertTrue(am.getContact().matches("sip:\\*8\\d@presence.server.com:\\d+"));
        }

        coreContextCtrl.verify();
    }

    public void testGetPresenceServerUri() {
        assertEquals("sip:presence.server.com:5130", m_server.getPresenceServerUri());
    }

    public void testGetMusicOnHoldUri() {
        m_server.setMohUser("moh");
        assertEquals("sip:moh@10.2.3.4:5120", m_server.getMusicOnHoldUri());
    }

    public void testSetRegistrarDomainAliases() {
        String[] aliases = {
            "example.com", "example.org"
        };
        m_server.setRegistrarDomainAliases(Arrays.asList(aliases));
        assertEquals("${MY_FULL_HOSTNAME} ${MY_IP_ADDR} example.com example.org", m_server
                .getSettingValue("domain/SIP_REGISTRAR_DOMAIN_ALIASES"));
        m_server.setRegistrarDomainAliases(null);
        assertEquals("${MY_FULL_HOSTNAME} ${MY_IP_ADDR}", m_server
                .getSettingValue("domain/SIP_REGISTRAR_DOMAIN_ALIASES"));
    }
}
