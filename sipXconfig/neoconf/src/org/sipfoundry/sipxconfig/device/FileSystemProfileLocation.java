/*
 * 
 * 
 * Copyright (C) 2007 SIPfoundry Inc.
 * Licensed by SIPfoundry under the LGPL license.
 * 
 * Copyright (C) 2007 Pingtel Corp.
 * Licensed to SIPfoundry under a Contributor Agreement.
 * 
 * $
 */
package org.sipfoundry.sipxconfig.device;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

public class FileSystemProfileLocation implements ProfileLocation {
    static final Log LOG = LogFactory.getLog(ProfileLocation.class);

    private String m_parentDir;

    public void setParentDir(String parentDir) {
        m_parentDir = parentDir;
    }

    public OutputStream getOutput(String profileName) {
        File profileFile = getProfileFile(profileName);
        ProfileUtils.makeParentDirectory(profileFile);
        try {
            return new BufferedOutputStream(new FileOutputStream(profileFile));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public void removeProfile(String profileName) {
        try {
            File file = getProfileFile(profileName);
            FileUtils.forceDelete(file);
        } catch (IOException e) {
            // ignore delete failure
            LOG.info(e.getMessage());
        }
    }

    private File getProfileFile(String profileName) {
        String profileFilename = FilenameUtils.concat(m_parentDir, profileName);
        File profileFile = new File(profileFilename);
        return profileFile;
    }
}
