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
package org.sipfoundry.sipxconfig.components;

import java.io.Serializable;

import org.apache.tapestry.services.DataSqueezer;
import org.sipfoundry.sipxconfig.common.DataObjectSource;
import org.sipfoundry.sipxconfig.common.PrimaryKeySource;

/**
 * Marshal/Unmarshal object from dao to tapestry based on ids.
 * 
 * FIXME: All beans that implement PrimaryKeySource use the same instance.  Unclear
 * if there's a way to organize squeezers by context.
 */
public class DaoSqueezeAdapter implements IDaoSqueezeAdapter {
    /** 1 character safest choice, read code in DataSqueezerImpl.register for rationale */
    private static final String PREFIX = "C"; // Arbitrary 
    private static final String DELIM = ":";
    private DataObjectSource m_dao;

    public void setDataObjectSource(DataObjectSource dao) {
        m_dao = dao;
    }

    public DataObjectSource getDataObjectSource() {
        return m_dao;
    }

    /**
     * Anything will do besides following
     * http://wiki.apache.org/jakarta-tapestry/HibernateTapestrySqueezer?highlight=%28hibernate%29
     */
    public String getPrefix() {
        return PREFIX;
    }

    public Class getDataClass() {
        return PrimaryKeySource.class;
    }

    public String squeeze(DataSqueezer squeezer, Object data) {
        String className = data.getClass().getCanonicalName();
        PrimaryKeySource p = (PrimaryKeySource) data;
        Object key = squeezeKey(p.getPrimaryKey());
        return getPrefix() + DELIM + className + DELIM + key.toString();
    }

    protected String squeezeKey(Object key) {
        if (!(key instanceof Integer)) {
            throw new RuntimeException("Default DaoSqueezeAdapter only "
                    + "supports Integer primary keys");
        }
        return key.toString();
    }

    protected Serializable unsqueezeKey(String skey) {
        return new Integer(skey);
    }

    public Object unsqueeze(DataSqueezer squeezer, String string) {
        String[] strings = string.split(DELIM);
        Serializable key = unsqueezeKey(strings[2]);
        Class c;
        try {
            c = Class.forName(strings[1]);
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
        // FIXME: Change last parameter to serializabe when interface changes.
        return getDataObjectSource().load(c, (Integer) key);
    }
}
