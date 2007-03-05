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
package org.sipfoundry.sipxconfig.vm;


public class DistributionList {
    // 0-9, not # or *
    private static final int MAX_SIZE = 10;
    private String[] m_extensions;
    
    public static DistributionList[] createBlankList() {
        DistributionList[] lists = new DistributionList[MAX_SIZE];
        for (int i = 0; i < lists.length; i++) {
            lists[i] = new DistributionList();
        }        
        return lists;
    }
    
    public String[] getExtensions() {
        return m_extensions;
    }
    
    public void setExtensions(String[] extensions) {
        m_extensions = extensions;
    }
}