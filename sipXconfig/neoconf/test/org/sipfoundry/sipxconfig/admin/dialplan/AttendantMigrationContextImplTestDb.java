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

import org.dbunit.Assertion;
import org.dbunit.dataset.ITable;
import org.dbunit.dataset.ReplacementDataSet;
import org.dbunit.dataset.filter.DefaultColumnFilter;
import org.sipfoundry.sipxconfig.SipxDatabaseTestCase;
import org.sipfoundry.sipxconfig.TestHelper;
import org.sipfoundry.sipxconfig.setting.Group;
import org.springframework.context.ApplicationContext;

public class AttendantMigrationContextImplTestDb extends SipxDatabaseTestCase {

    private AttendantMigrationContext m_context;
    private DialPlanContext m_dialPlanContext;

    protected void setUp() throws Exception {
        ApplicationContext appContext = TestHelper.getApplicationContext();
        m_context = (AttendantMigrationContext) appContext
                .getBean(AttendantMigrationContext.CONTEXT_BEAN_NAME);
        m_dialPlanContext = (DialPlanContext) appContext
                .getBean(DialPlanContext.CONTEXT_BEAN_NAME);
        TestHelper.cleanInsert("ClearDb.xml");
    }

    public void testMigrateAttendantRules() throws Exception {
        TestHelper.cleanInsertFlat("admin/dialplan/pre_attendant_migration.db.xml");
        m_context.migrateAttendantRules();

        ITable actual = getConnection().createDataSet().getTable("attendant_dialing_rule");
        assertEquals(2, actual.getRowCount());

        ITable expected = TestHelper.loadDataSetFlat(
                "admin/dialplan/post_attendant_migration.xml").getTable(
                "attendant_dialing_rule");

        ITable filteredTable = DefaultColumnFilter.includedColumnsTable(actual, expected
                .getTableMetaData().getColumns());

        Assertion.assertEquals(expected, filteredTable);
    }
    
    public void testSetAttendantDefaults() throws Exception {
        TestHelper.cleanInsertFlat("admin/dialplan/seedAttendant.xml");
        Group defaults = m_dialPlanContext.getDefaultAutoAttendantGroup();
        m_context.setAttendantDefaults();
        ReplacementDataSet expected = TestHelper.loadReplaceableDataSetFlat(
                "admin/dialplan/set_attendant_defaults.db.xml");
        expected.addReplacementObject("[group_id]", defaults.getId());
        ITable actual = getConnection().createDataSet().getTable("attendant_group");
        Assertion.assertEquals(expected.getTable("attendant_group"), actual);
    }
}