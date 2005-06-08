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
package org.sipfoundry.sipxconfig.site.dialplan;

import junit.framework.Test;
import net.sourceforge.jwebunit.WebTestCase;

import org.sipfoundry.sipxconfig.site.SiteTestHelper;

/**
 * DialingRuleEditTestUi
 */
public class DialingRuleTestUi extends WebTestCase {
    public static Test suite() throws Exception {
        return SiteTestHelper.webTestSuite(DialingRuleTestUi.class);
    }

    public void setUp() {
        getTestContext().setBaseUrl(SiteTestHelper.getBaseUrl());
        SiteTestHelper.home(getTester());
        clickLink("resetDialPlans");
    }

    public void testAddExistingGateway() {
        GatewaysTestUi.addTestGateways(getTester(), 3);
        SiteTestHelper.home(getTester());
        clickLink("FlexibleDialPlan");
        SiteTestHelper.assertNoException(getTester());
        clickLinkWithText("Emergency");
        //it's a submit link: uses java script, does not have id
        clickLinkWithText("Add Existing Gateway");
    }
}
