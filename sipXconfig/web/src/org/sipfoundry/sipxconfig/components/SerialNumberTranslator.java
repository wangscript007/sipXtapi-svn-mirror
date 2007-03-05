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

import org.apache.tapestry.form.IFormComponent;
import org.apache.tapestry.form.ValidationMessages;
import org.apache.tapestry.form.translator.StringTranslator;
import org.sipfoundry.sipxconfig.phone.Phone;

/**
 * Converts an input field to lowercase in tapestry
 */
public class SerialNumberTranslator extends StringTranslator {

    protected Object parseText(IFormComponent field, ValidationMessages messages, String text) {
        return Phone.cleanSerialNumber(text);
    }
}