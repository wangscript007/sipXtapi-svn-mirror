/*
 * 
 * 
 * Copyright (C) 2004 SIPfoundry Inc.
 * Licensed by SIPfoundry under the LGPL license.
 * 
 * Copyright (C) 2004 Pingtel Corp.
 * Licensed to SIPfoundry under a Contributor Agreement.
 * 
 * $
 */
package org.sipfoundry.sipxconfig.common;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URL;
import java.security.CodeSource;
import java.util.Properties;

import org.apache.commons.lang.StringUtils;

/**
 * Common utility methods for unittesting with neoconf.  Somewhat controversial being
 * in codebase, however unittest library is uglier IMHO.
 */
public final class TestUtil {

    public static final String APPLICATION_CONTEXT_FILE = "org/sipfoundry/sipxconfig/applicationContext-sipxconfig.xml";
    
    private TestUtil() {        
    }

    /**
     * The directory that is part of the classpath that a class was loaded from 
     */
    public static String getClasspathDirectory(Class testClass) {
        // create file on classpath
        CodeSource code = testClass.getProtectionDomain().getCodeSource();
        URL classpathUrl = code.getLocation();
        File classpathDir = new File(classpathUrl.getFile());

        return classpathDir.getAbsolutePath();
    }
    
    /**
     * Where to direct test output, cleaned on 'ant clean' and ignored by subversion
     */
    public static String getTestOutputDirectory(String project) {
        return getBuildDirectory(project)  + "/test-output";
    }
    
    /**
     * Use ClassLoader.getSystemResource() when you can gets you a stream and can work
     * from jars, but when you need a filename use this.
     * Example:
     * <pre>
     * # Test file in same directory as JUnit test source file 
     * String testFile = TestUtil.getTestSourceDirectory(getClass()) + "/test-file";
     * </pre>
     */
    public static String getTestSourceDirectory(Class testClass) {
        StringBuffer sb = new StringBuffer(TestUtil.getProjectDirectory())
            .append("/test/")
            .append(testClass.getPackage().getName().replace('.', '/'));
        String path = sb.toString();
        
        return path;
    }

    public static String getProjectDirectory() {
        // eclipse        
        String userDir = System.getProperty("user.dir");
        // ant
        return System.getProperty("basedir", userDir);
    }
    
    /**
     * Get the directory all autoconf and ant build output gets sent
     */
    public static String getBuildDirectory(String project) {
        try {
            int depth = StringUtils.countMatches(project, "/") + 1;
            File topSrcDir = new File(getProjectDirectory());
            for (int i = 0; i < depth; i++) {
                topSrcDir = topSrcDir.getParentFile();
            }
            String propName = "top.build.dir";
            File propFile = new File(topSrcDir, propName);
            Properties props = new Properties();
            FileInputStream topBuildDirProperties = new FileInputStream(propFile);
            props.load(topBuildDirProperties);

            return props.getProperty(propName) + '/' + project;
        } catch (IOException ioe) {
            throw new RuntimeException("Could not find top build directory", ioe);
        }
    }

    /**
     * Create a sysdir.properties file in the classpath.  Uses a trick that
     * will only work if unittests are unjar-ed.  This is infavor of doing
     * in ant because it avoids setup and works in IDE's like eclipse where
     * bin.eclipse is the classpath
     */
    public static Properties getSysDirProperties(Properties sysProps, String classpathDirectory, String etcDirectory,
            String outputDirectory) {

        // HACK: sysdir.bin is not a real directory when testing
        sysProps.setProperty("sysdir.bin", "/opt/work-2.8/sipx/bin/");
        sysProps.setProperty("sysdir.etc", etcDirectory);
        sysProps.setProperty("sysdir.data", outputDirectory);
        sysProps.setProperty("sysdir.phone", outputDirectory);
        sysProps.setProperty("sysdir.log", outputDirectory);
        sysProps.setProperty("dataSource.url", "jdbc:postgresql://localhost/PDS_TEST");
        sysProps.setProperty("polycom300.outboundProxyAddress", "proxy.sipfoundry.org");

        File sysdirPropsFile = new File(classpathDirectory, "sipxconfig.properties");
        FileOutputStream sysdirPropsStream;
        try {
            sysdirPropsStream = new FileOutputStream(sysdirPropsFile);
            // store them so spring's application context file find it
            // in classpath
            sysProps.store(sysdirPropsStream, null);
        } catch (FileNotFoundException e) {
            throw new RuntimeException("could not create system dir properties file", e);
        } catch (IOException e) {
            throw new RuntimeException("could not store system dir properties", e);
        }

        return sysProps;
    }
}
