//
//  kern_start.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_config.hpp"
#include "kern_ngfx.hpp"

static NGFX ngfx;

const char *Configuration::bootargOff[] {
	"-ngfxoff"
};

const char *Configuration::bootargDebug[] {
	"-ngfxdbg"
};

const char *Configuration::bootargBeta[] {
	"-ngfxbeta"
};

Configuration config;

void Configuration::readArguments() {
    char tmp[20];
    if (PE_parse_boot_argn(bootargNoAudioCon, tmp, sizeof(tmp)))
        noaudioconnectors = true;
    
    if (PE_parse_boot_argn(bootargNoAudio, tmp, sizeof(tmp)))
        noaudiofixes = true;
    
    if (PE_parse_boot_argn(bootargNoVARenderer, tmp, sizeof(tmp)))
        novarenderer = true;
    
    if (PE_parse_boot_argn(bootargNoVARenderer, tmp, sizeof(tmp)))
        novarenderer = true;
    
    if (PE_parse_boot_argn(bootargNoLibValFix, tmp, sizeof(tmp)))
        nolibvalfix = true;
    
    if (PE_parse_boot_argn(bootargPatchList, patch_list, sizeof(patch_list)))
    {
        DBGLOG("ngfx", "boot-arg %s specified, value = %s", bootargPatchList, patch_list);
    }
    else
        DBGLOG("ngfx", "use default value = %s for patch list", patch_list);
}


PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery,
	config.bootargOff,
	arrsize(config.bootargOff),
	config.bootargDebug,
	arrsize(config.bootargDebug),
	config.bootargBeta,
	arrsize(config.bootargBeta),
	KernelVersion::MountainLion,
	KernelVersion::HighSierra,
	[]() {
        config.readArguments();
		ngfx.init();
	}
};





