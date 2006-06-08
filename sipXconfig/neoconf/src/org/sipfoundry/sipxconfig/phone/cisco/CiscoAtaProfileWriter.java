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
package org.sipfoundry.sipxconfig.phone.cisco;

import java.io.IOException;
import java.io.Writer;
import java.util.Iterator;
import java.util.List;

import org.apache.commons.lang.StringUtils;
import org.sipfoundry.sipxconfig.phone.Line;
import org.sipfoundry.sipxconfig.phone.LineInfo;
import org.sipfoundry.sipxconfig.setting.Setting;
import org.sipfoundry.sipxconfig.setting.SettingFilter;
import org.sipfoundry.sipxconfig.setting.SettingUtil;
import org.sipfoundry.sipxconfig.setting.SettingVisitor;

public class CiscoAtaProfileWriter implements SettingVisitor {
    private static final char LF = 0x0a;
    private static final String UPGRADE_SETTING_GROUP = "_upgrade";
    private static final String ZERO = "0";
    private static final String IMAGE_ID = "imageid";
    private static final String NONE = "none";
    private Writer m_wtr;
    private String m_profileNameSuffix = StringUtils.EMPTY;
    private int m_lineIndex;

    public CiscoAtaProfileWriter(Writer wtr) {
        m_wtr = wtr;
    }

    public void write(CiscoAtaPhone phone) {
        writePhone(phone);
        for (Line l : phone.getProfileLines()) {
            writeLine(l);
        }
    }

    void writePhone(CiscoAtaPhone phone) {
        writeHeader();
        m_profileNameSuffix = StringUtils.EMPTY;
        Setting settings = phone.getSettings();
        settings.acceptVisitor(this);
        writeProxyConfig(phone);
        writeSoftwareUpgradeConfig(phone);
        writeLogoUpgradeConfig(phone);
        writeCountyDialTones(phone);
    }
    
    void writeHeader() {
        try {
            m_wtr.append("#txt");
            m_wtr.append(LF);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    void writeLine(Line line) {
        m_profileNameSuffix = String.valueOf(m_lineIndex);
        Setting settings = line.getSettings();
        settings.acceptVisitor(this);
        m_lineIndex++;
    }

    void writeCountyDialTones(CiscoAtaPhone phone) {
        Setting country = phone.getSettings().getSetting("country");
        String countryCode = country.getSetting("_country").getValue();
        String countryModelId = phone.getCiscoModel().isAta() ? "ata" : "79xx";
        String tonePath = "_tone." + countryCode + '.' + countryModelId;
        Setting tones = country.getSetting(tonePath);
        if (tones != null) {
            Iterator i = tones.getValues().iterator();
            while (i.hasNext()) {
                Setting toneSetting = (Setting) i.next();
                writeSetting(toneSetting);
            }
        }
    }

    void writeProxyConfig(CiscoAtaPhone phone) {
        List lines = phone.getLines();
        if (lines.size() == 0) {
            return;
        }

        Line line = (Line) lines.get(0);
        LineInfo info = line.getLineInfo();
        String entry = info.getRegistrationServer() + ':' + info.getRegistrationServerPort();
        writeEntry("Proxy", entry);
    }

    public void writeSoftwareUpgradeConfig(CiscoAtaPhone phone) {
        Setting swupgrade = phone.getSettings().getSetting(UPGRADE_SETTING_GROUP);
        if (swupgrade == null) {
            return;
        }
        String cfg = phone.getCiscoModel().getCfgPrefix();
        String swimage = swupgrade.getSetting("upgradecode." + cfg).getValue();
        String imageid = swupgrade.getSetting(IMAGE_ID + '.' + cfg).getValue();
        CiscoModel model = (CiscoModel) phone.getModel();
        String upghex = model.getUpgCode();

        if (StringUtils.isBlank(swimage) || swimage.equals(NONE) || imageid.equals(ZERO)) {
            return;
        }
        String value = String.format("3,%s,0.0.0.0,69,%s,%s", upghex, imageid, swimage);

        writeEntry("upgradecode", value);
    }

    public void writeLogoUpgradeConfig(CiscoAtaPhone phone) {
        if (phone.getCiscoModel().isAta()) {
            return;
        }

        Setting logoupgrade = phone.getSettings().getSetting(UPGRADE_SETTING_GROUP);

        if (logoupgrade == null) {
            return;
        }

        String logofile = logoupgrade.getSetting("logofile").getValue();
        String imageid = logoupgrade.getSetting("logoid").getValue();

        if (StringUtils.isBlank(logofile) || logofile.equals(NONE) || imageid.equals(ZERO)) {
            return;
        }

        writeEntry("upgradelogo", imageid + ",0," + logofile);
    }

    public void visitSetting(Setting setting) {
        String profileName = setting.getProfileName();
        if (Bitmap.isBitmask(profileName)) {
            writeBitMask(setting);
        // bitfields written has partof bitmask
        } else if (!Bitmap.isBitField(profileName) && !isVirtual(setting)) {
            writeSetting(setting);
        }
    }

    void writeSetting(Setting setting) {
        writeEntry(setting.getProfileName(), setting.getValue());
    }

    boolean isVirtual(Setting setting) {
        Setting s = setting;
        while (s != null) {
            if (s.getName().startsWith("_")) {
                return true;
            }
            s = s.getParent();
        }
        return false;
    }

    void writeBitMask(Setting setting) {
        Bitmap bitMask = new Bitmap(setting);
        Iterator i = SettingUtil.filter(bitMask, setting.getParent()).iterator();
        while (i.hasNext()) {
            Setting bitFieldSetting = (Setting) i.next();
            bitMask.setBitField(bitFieldSetting);
        }
        writeEntry(bitMask.getProfileName(), bitMask.getProfileValue());
    }

    void writeEntry(String name, String value) {
        try {
            m_wtr.append(name);
            m_wtr.append(m_profileNameSuffix);
            m_wtr.append(':');
            if (StringUtils.isNotBlank(value)) {
                m_wtr.append(value);
            }
            m_wtr.append(LF);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    static class Bitmap implements SettingFilter {
        private static final String BITMASK = ".BM.";
        private static final String BITFIELD = ".BF.";

        private String m_name;
        private long m_bitmask;

        public Bitmap(Setting bitmaskSetting) {
            String profileName = bitmaskSetting.getProfileName();
            m_name = profileName.substring(BITMASK.length());
            bitmaskSetting.getTypedValue();
            String value = bitmaskSetting.getValue();
            if (StringUtils.isNotBlank(value)) {
                m_bitmask = Long.decode(value);
            }
        }

        public static boolean isBitmask(String profileName) {
            return profileName.startsWith(BITMASK);
        }

        public static boolean isBitField(String profileName) {
            return profileName.startsWith(BITFIELD);
        }

        public String getProfileValue() {
            return "0x" + Long.toHexString(m_bitmask);
        }

        public String getProfileName() {
            return m_name;
        }

        public void setBitField(Setting bitField) {
            String profileName = bitField.getProfileName();
            String[] fields = profileName.substring(1).split("\\.");
            int shift = Integer.parseInt(fields[1]);
            String svalue = StringUtils.defaultString(bitField.getValue(), ZERO);
            long value = Integer.parseInt(svalue);
            long shiftedValue = value << shift;

            // NOTE Because this is a OR, applying a value only works once and
            // original value would have to start out zero in respective bits 
            m_bitmask = m_bitmask | shiftedValue;
        }

        public boolean acceptSetting(Setting root_, Setting setting) {
            String name = setting.getProfileName();
            boolean isBitField = isBitField(name);
            boolean isThisBitField = name.endsWith(m_name);
            return isBitField && isThisBitField;
        }
    }

    public void visitSettingGroup(Setting group) {
    }
}
