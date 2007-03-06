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

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

import org.apache.tapestry.IAsset;
import org.apache.tapestry.asset.AssetFactory;
import org.springframework.beans.factory.BeanFactory;
import org.springframework.beans.factory.BeanFactoryAware;
import org.springframework.context.MessageSource;
import org.springframework.context.NoSuchMessageException;

/**
 * UI control such as stylesheet assets to change skin
 */
public class SkinControl implements BeanFactoryAware {
    public static final String CONTEXT_BEAN_NAME = "skin";
    private static final String ASSET_COLORS = "colors.css";
    private static final String ASSET_LAYOUT = "layout.css";
    private TapestryContext m_tapestryContext;
    // overrideable in skin
    private Map<String, String> m_assets = new HashMap();
    private String m_messageSourceBeanId;
    private MessageSource m_messageSource;
    private BeanFactory m_beanFactory;

    public SkinControl() {
        // default skin resources
        m_assets.put("logo.png", "org/sipfoundry/sipxconfig/components/sipxconfig-logo.png");
        m_assets.put(ASSET_LAYOUT, "org/sipfoundry/sipxconfig/components/layout.css");
        m_assets.put(ASSET_COLORS, "org/sipfoundry/sipxconfig/components/colors.css");
    }

    public IAsset[] getStylesheetAssets() {
        IAsset[] assets = new IAsset[2];
        assets[0] = getAsset(ASSET_COLORS);
        assets[1] = getAsset(ASSET_LAYOUT);
        return assets;
    }

    private AssetFactory getAssetFactory() {
        return m_tapestryContext.getHivemindContext().getClasspathAssetFactory();
    }

    public Map<String, String> getAssets() {
        return m_assets;
    }

    public void setAssets(Map<String, String> assets) {
        m_assets.putAll(assets);
    }

    public IAsset getAsset(String path) {
        String resourcePath = m_assets.get(path);
        if (resourcePath == null) {
            return null;
        }

        return getAssetFactory().createAbsoluteAsset(resourcePath, null, null);
    }

    public void setTapestryContext(TapestryContext tapestryContext) {
        m_tapestryContext = tapestryContext;
    }
    
    /**
     * To override any resource string
     */
    public void setMessageSourceBeanId(String messageSourceBeanId) {
        m_messageSourceBeanId = messageSourceBeanId;
    }
    
    public String getLocalizeString(String component, String key, Locale locale, String defaultString) {
        if (m_messageSourceBeanId == null) {
            return defaultString;
        }

        if (m_messageSource == null) {
            m_messageSource = (MessageSource) m_beanFactory.getBean(m_messageSourceBeanId);
        }
        try {
            // pass null for message args, tapestry will use MessageFormat later.
            return m_messageSource.getMessage(key, null, locale);
        } catch (NoSuchMessageException extremelyLikely) {            
            return defaultString;
        }
    }

    public void setBeanFactory(BeanFactory beanFactory) {
        m_beanFactory = beanFactory;
    }
}
