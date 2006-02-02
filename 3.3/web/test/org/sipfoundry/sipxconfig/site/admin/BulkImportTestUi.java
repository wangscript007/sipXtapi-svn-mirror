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
package org.sipfoundry.sipxconfig.site.admin;

import junit.framework.Test;
import net.sourceforge.jwebunit.WebTestCase;

import org.sipfoundry.sipxconfig.site.SiteTestHelper;

public class BulkImportTestUi extends WebTestCase {
    public static Test suite() throws Exception {
        return SiteTestHelper.webTestSuite(BulkImportTestUi.class);
    }

    public void setUp() {
        getTestContext().setBaseUrl(SiteTestHelper.getBaseUrl());
        SiteTestHelper.home(getTester());
        clickLink("link:import");
    }

    /**
     * Basic display test
     */
    public void testDisplay() {
        SiteTestHelper.assertNoException(getTester());
        assertFormPresent("import:form");
        assertElementPresent("import:upload");
        assertButtonPresent("import:ok");
    }
}
