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
package org.sipfoundry.sipxconfig.admin.dialplan;

import java.util.List;

import org.sipfoundry.sipxconfig.admin.dialplan.config.Transform;

/**
 * InternalRule
 */
public class InternalRule extends DialingRule {
    private static final String DEFAULT_VMAIL_PREFIX = "8";
    private static final int DEFAULT_LOCAL_EXT_LEN = 3;

    private String m_voiceMailPrefix = DEFAULT_VMAIL_PREFIX;
    private int m_localExtensionLen = DEFAULT_LOCAL_EXT_LEN;
    private String m_autoAttendant = "100";
    private String m_voiceMail = "101";

    public String[] getPatterns() {
        return null;
    }

    public Transform[] getTransforms() {
        return null;
    }

    public Type getType() {
        return Type.INTERNAL;
    }

    public String getAutoAttendant() {
        return m_autoAttendant;
    }

    public void setAutoAttendant(String autoAttendant) {
        m_autoAttendant = autoAttendant;
    }

    public int getLocalExtensionLen() {
        return m_localExtensionLen;
    }

    public void setLocalExtensionLen(int localExtensionLen) {
        m_localExtensionLen = localExtensionLen;
    }

    public String getVoiceMail() {
        return m_voiceMail;
    }

    public void setVoiceMail(String voiceMail) {
        m_voiceMail = voiceMail;
    }

    public String getVoiceMailPrefix() {
        return m_voiceMailPrefix;
    }

    public void setVoiceMailPrefix(String voiceMailPrefix) {
        m_voiceMailPrefix = voiceMailPrefix;
    }

    public void appendToGenerationRules(List rules) {
        if (isEnabled()) {
            rules.add(new MappingRule.Operator(m_autoAttendant));
            rules.add(new MappingRule.Voicemail(m_voiceMail));
            rules.add(new MappingRule.VoicemailTransfer(m_voiceMailPrefix, m_localExtensionLen));
            rules.add(new MappingRule.VoicemailFallback(m_localExtensionLen));
        }
    }
}
