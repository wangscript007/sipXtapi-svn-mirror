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
package org.sipfoundry.sipxconfig.gateway.audiocodes;

import java.io.File;
import java.io.StringWriter;
import java.io.Writer;

import junit.framework.TestCase;

import org.easymock.MockControl;
import org.easymock.classextension.MockClassControl;
import org.sipfoundry.sipxconfig.TestHelper;
import org.sipfoundry.sipxconfig.phone.PhoneDefaults;
import org.sipfoundry.sipxconfig.setting.Setting;
import org.sipfoundry.sipxconfig.setting.SettingSet;

public class MediantGatewayTest extends TestCase {
    private AudioCodesModel m_model;
    private MediantGateway m_gateway;

    protected void setUp() throws Exception {
        m_model = (AudioCodesModel) TestHelper.getApplicationContext().getBean(
                "gmAudiocodesMP1X4_4_FXO");
        m_gateway = (MediantGateway) TestHelper.getApplicationContext().getBean(
                "gwAudiocodesMediant");
        m_gateway.setBeanId(m_model.getBeanId());
        m_gateway.setModelId(m_model.getModelId());
    }

    public void testGenerateProfiles() throws Exception {
        assertSame(m_model, m_gateway.getModel());

        Writer writer = new StringWriter();
        m_gateway.generateProfiles(writer);

        // cursory check for now
        assertTrue(writer.toString().indexOf("MAXDIGITS") >= 0);
    }

    public void testGenerateAndRemoveProfiles() throws Exception {
        File file = m_gateway.getIniFile();
        m_gateway.generateProfiles();
        assertTrue(file.exists());
        m_gateway.removeProfiles();
        assertFalse(file.exists());
    }

    public void testPrepareSettings() throws Exception {
        assertSame(m_model, m_gateway.getModel());

        MockControl defaultsCtrl = MockClassControl.createControl(PhoneDefaults.class);
        PhoneDefaults defaults = (PhoneDefaults) defaultsCtrl.getMock();
        defaults.getDomainName();
        defaultsCtrl.setReturnValue("mysipdomain.com");
        defaults.getOutboundProxy();
        defaultsCtrl.setReturnValue("10.1.2.3");
        defaults.getOutboundProxyPort();
        defaultsCtrl.setReturnValue("4321");

        defaultsCtrl.replay();

        m_gateway.setDefaults(defaults);
        m_gateway.prepareSettings();

        assertEquals("10.1.2.3:4321", m_gateway.getSettingValue("SIP_Params/PROXYIP"));
        assertEquals("mysipdomain.com", m_gateway.getSettingValue("SIP_Params/PROXYNAME"));

        defaultsCtrl.verify();
    }

    public void testGetSettings() throws Exception {
        Setting settings = m_gateway.getSettings();
        assertEquals("15", settings.getSetting("SIP_Params/MAXDIGITS").getValue());
        assertTrue(settings instanceof SettingSet);
        SettingSet root = (SettingSet) settings;
        SettingSet currentSettingSet = (SettingSet) root.getDefaultSetting(SettingSet.class);
        assertEquals("15", currentSettingSet.getSetting("MAXDIGITS").getValue());
    }
}
