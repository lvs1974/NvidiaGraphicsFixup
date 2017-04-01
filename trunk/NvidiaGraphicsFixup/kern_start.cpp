//
//  kern_start.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_ngfx.hpp"

static NGFX ngfx;

static const char *bootargOff[] {
	"-ngfxoff"
};

static const char *bootargDebug[] {
	"-ngfxdbg"
};

static const char *bootargBeta[] {
	"-ngfxbeta"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
	bootargOff,
	sizeof(bootargOff)/sizeof(bootargOff[0]),
	bootargDebug,
	sizeof(bootargDebug)/sizeof(bootargDebug[0]),
	bootargBeta,
	sizeof(bootargBeta)/sizeof(bootargBeta[0]),
	KernelVersion::MountainLion,
	KernelVersion::Sierra,
	[]() {
		ngfx.init();
	}
};





