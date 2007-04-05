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
package org.sipfoundry.sipxconfig.setting;

public class SettingValueImpl implements SettingValue {
    private String m_value;
    
    public SettingValueImpl(String value) {
        m_value = value;
    }
    
    public String getValue() {
        return m_value;
    }
    
    @Override
    public String toString() {
        return m_value; 
    }
    
    @Override
    public int hashCode() {
        return m_value == null ? 0 : m_value.hashCode();
    }
    
    @Override
    public boolean equals(Object ovalue) {
        if (!(ovalue instanceof SettingValue)) {
            return false;
        }
        
        SettingValue value = (SettingValue) ovalue;
        if (value == null) {
            return false;
        }
        String svalue = value.getValue();
        if (m_value == null) {
            return svalue == null;
        }
        
        return m_value.equals(svalue);        
    }
}
