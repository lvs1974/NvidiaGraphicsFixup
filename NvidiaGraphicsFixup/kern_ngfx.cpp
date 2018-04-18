//
//  kern_ngfx.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_iokit.hpp>

#include <sys/types.h>
#include <sys/sysctl.h>

#include "kern_config.hpp"
#include "kern_ngfx.hpp"


static const char *kextAGDPolicy[] {
	"/System/Library/Extensions/AppleGraphicsControl.kext/Contents/PlugIns/AppleGraphicsDevicePolicy.kext/Contents/MacOS/AppleGraphicsDevicePolicy"
};
static const char *kextGeForce[] {
	"/System/Library/Extensions/GeForce.kext/Contents/MacOS/GeForce"
};
static const char *kextGeForceWeb[] {
	"/Library/Extensions/GeForceWeb.kext/Contents/MacOS/GeForceWeb",
	"/System/Library/Extensions/GeForceWeb.kext/Contents/MacOS/GeForceWeb"
};
static const char *kextNVDAStartupWeb[] {
	"/Library/Extensions/NVDAStartupWeb.kext/Contents/MacOS/NVDAStartupWeb",
	"/System/Library/Extensions/NVDAStartupWeb.kext/Contents/MacOS/NVDAStartupWeb"
};

static KernelPatcher::KextInfo kextList[] {
	{ "com.apple.driver.AppleGraphicsDevicePolicy",     kextAGDPolicy,   	arrsize(kextAGDPolicy),  	 {true}, {}, KernelPatcher::KextInfo::Unloaded },
	{ "com.apple.GeForce",        						kextGeForce,     	arrsize(kextGeForce),    	 {},     {}, KernelPatcher::KextInfo::Unloaded },
	{ "com.nvidia.web.GeForceWeb",     					kextGeForceWeb,  	arrsize(kextGeForceWeb), 	 {},     {}, KernelPatcher::KextInfo::Unloaded },
	{ "com.nvidia.NVDAStartupWeb",						kextNVDAStartupWeb, arrsize(kextNVDAStartupWeb), {},	 {}, KernelPatcher::KextInfo::Unloaded }
};

enum : size_t {
	KextAGDPolicy,
	KextGeForce,
	KextGeForceWeb,
	KextNVDAStartupWeb
};

static size_t kextListSize {arrsize(kextList)};

// Only used in apple-driven callbacks
static NGFX *callbackNGFX = nullptr;

// Used by asm code
bool (*orgVaddrPresubmitOfficial)(void *addr) = nullptr;

bool NGFX::init() {
	// The code below (enabled by ngfxcompat=0) is not only relevant for a not so important optimisation
	// to avoid loading NVDAStartupWeb from HDD but actually reduces the crash rate of a nasty memory
	// corruption existing in the latest NVIDIA Pascal Web driver (yes, it is not a NvidiaGraphicsFixup bug).
	// https://i.applelife.ru/2018/04/427585_Panic.txt
	// https://i.applelife.ru/2018/04/427558_FCP.txt
	if (config.force_compatibility == 0) {
		kextList[KextNVDAStartupWeb].pathNum = 0;
		progressState |= ProcessingState::NVDAStartupWebRouted;
	}

	LiluAPI::Error error = lilu.onPatcherLoad(
	  [](void *user, KernelPatcher &patcher) {
		  callbackNGFX = static_cast<NGFX *>(user);
		  callbackNGFX->processKernel(patcher);
	  }, this);

	if (error != LiluAPI::Error::NoError) {
		SYSLOG("ngfx", "failed to register onPatcherLoad method %d", error);
		return false;
	}

	error = lilu.onKextLoad(kextList, kextListSize,
		[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
			callbackNGFX = static_cast<NGFX *>(user);
			callbackNGFX->processKext(patcher, index, address, size);
		}, this);

	if (error != LiluAPI::Error::NoError) {
		SYSLOG("ngfx", "failed to register onKextLoad method %d", error);
		return false;
	}

	return true;
}

void NGFX::deinit() {
}

void NGFX::processKernel(KernelPatcher &patcher) {
	if (!(progressState & ProcessingState::KernelRouted)) {
		if (!strcmp(config.patch_list, "detect")) {
			lilu_os_strncpy(config.patch_list, "vit9696", sizeof(config.patch_list));
			char boardIdentifier[64] {};
			if (WIOKit::getComputerInfo(nullptr, 0, boardIdentifier, sizeof(boardIdentifier))) {
				// We do not need AGDC patches on compatible devices.
				// On native hardware they are harmful, since they may kill IGPU support.
				// See https://github.com/lvs1974/NvidiaGraphicsFixup/issues/13
				const char *compatibleBoards[] {
					"Mac-00BE6ED71E35EB86", // iMac13,1
					"Mac-27ADBB7B4CEE8E61", // iMac14,2
					"Mac-4B7AC7E43945597E", // MacBookPro9,1
					"Mac-77EB7D7DAF985301", // iMac14,3
					"Mac-C3EC7CD22292981F", // MacBookPro10,1
					"Mac-C9CF552659EA9913", // ???
					"Mac-F221BEC8",         // MacPro5,1 (and MacPro4,1)
					"Mac-F221DCC8",         // iMac10,1
					"Mac-F42C88C8",         // MacPro3,1
					"Mac-FC02E91DDD3FA6A4", // iMac13,2
					"Mac-2BD1B31983FE1663"  // MacBookPro11,3
				};
				for (size_t i = 0; i < arrsize(compatibleBoards); i++) {
					if (!strcmp(compatibleBoards[i], boardIdentifier)) {
						DBGLOG("ngfx", "disabling nvidia patches on model %s", boardIdentifier);
						kextList[KextAGDPolicy].pathNum = 0;
						progressState |= ProcessingState::GraphicsDevicePolicyPatched;
						config.patch_list[0] = '\0';
						break;
					}
				}
			}
		}


		if (getKernelVersion() > KernelVersion::Mavericks && getKernelVersion() < KernelVersion::HighSierra) {
			if (!config.nolibvalfix) {
				auto method_address = patcher.solveSymbol(KernelPatcher::KernelID, "_csfg_get_teamid");
				if (method_address) {
					DBGLOG("ngfx", "obtained _csfg_get_teamid");
					csfg_get_teamid = reinterpret_cast<t_csfg_get_teamid>(method_address);

					method_address = patcher.solveSymbol(KernelPatcher::KernelID, "_csfg_get_platform_binary");
					if (method_address ) {
						DBGLOG("ngfx", "obtained _csfg_get_platform_binary");
						patcher.clearError();
						org_csfg_get_platform_binary = reinterpret_cast<t_csfg_get_platform_binary>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(csfg_get_platform_binary), true));
						if (patcher.getError() == KernelPatcher::Error::NoError) {
							DBGLOG("ngfx", "routed _csfg_get_platform_binary");
						} else {
							SYSLOG("ngfx", "failed to route _csfg_get_platform_binary");
						}
					} else {
						SYSLOG("ngfx", "failed to resolve _csfg_get_platform_binary");
					}

				} else {
					SYSLOG("ngfx", "failed to resolve _csfg_get_teamid");
				}
			}
		}

		progressState |= ProcessingState::KernelRouted;
	}

	// Ignore all the errors for other processors
	patcher.clearError();
}

void NGFX::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (progressState != ProcessingState::EverythingDone) {
		for (size_t i = 0; i < kextListSize; i++) {
			if (kextList[i].loadIndex == index) {
				if (!(progressState & ProcessingState::GraphicsDevicePolicyPatched) && i == KextAGDPolicy)
				{
					DBGLOG("ngfx", "found %s", kextList[i].id);

					const bool patch_vit9696 = (strstr(config.patch_list, "vit9696") != nullptr);
					const bool patch_pikera  = (strstr(config.patch_list, "pikera")  != nullptr);
					const bool patch_cfgmap  = (strstr(config.patch_list, "cfgmap")  != nullptr);

					if (patch_vit9696)
					{
						const uint8_t find[]    = {0xBA, 0x05, 0x00, 0x00, 0x00};
						const uint8_t replace[] = {0xBA, 0x00, 0x00, 0x00, 0x00};
						KextPatch kext_patch {
							{&kextList[i], find, replace, sizeof(find), 1},
							KernelVersion::MountainLion, KernelPatcher::KernelAny
						};
						applyPatches(patcher, index, &kext_patch, 1, "vit9696");
					}

					if (patch_pikera)
					{
						const uint8_t find[]    = "board-id";
						const uint8_t replace[] = "board-ix";
						KextPatch kext_patch {
							{&kextList[i], find, replace, strlen((const char*)find), 1},
							KernelVersion::MountainLion, KernelPatcher::KernelAny
						};
						applyPatches(patcher, index, &kext_patch, 1, "pikera");
					}

					if (patch_cfgmap)
					{
						auto method_address = patcher.solveSymbol(index, "__ZN25AppleGraphicsDevicePolicy5startEP9IOService", address, size);
						if (method_address) {
							DBGLOG("ngfx", "obtained __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
							patcher.clearError();
							orgAgdpStart = reinterpret_cast<t_agdp_start>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(AppleGraphicsDevicePolicy_start), true));
							if (patcher.getError() == KernelPatcher::Error::NoError) {
								DBGLOG("ngfx", "routed __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
							} else {
								SYSLOG("ngfx", "failed to route __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
							}
						} else {
							SYSLOG("ngfx", "failed to resolve __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
						}
					}
					progressState |= ProcessingState::GraphicsDevicePolicyPatched;
				}
				else if (!(progressState & ProcessingState::GeForceRouted) && i == KextGeForce)
				{
					if (!config.novarenderer) {
						DBGLOG("ngfx", "found %s", kextList[i].id);
						auto method_address = patcher.solveSymbol(index, "__ZN13nvAccelerator18SetAccelPropertiesEv", address, size);
						if (method_address) {
							DBGLOG("ngfx", "obtained __ZN13nvAccelerator18SetAccelPropertiesEv");
							patcher.clearError();
							orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(nvAccelerator_SetAccelProperties), true));
							if (patcher.getError() == KernelPatcher::Error::NoError) {
								DBGLOG("ngfx", "routed __ZN13nvAccelerator18SetAccelPropertiesEv");
							} else {
								SYSLOG("ngfx", "failed to route __ZN13nvAccelerator18SetAccelPropertiesEv");
							}
						} else {
							SYSLOG("ngfx", "failed to resolve __ZN13nvAccelerator18SetAccelPropertiesEv");
						}
					}

					restoreLegacyOptimisations(patcher, index, address, size);

					progressState |= ProcessingState::GeForceRouted;
				}
				else if (!(progressState & ProcessingState::GeForceWebRouted) && i == KextGeForceWeb)
				{
					if (!config.novarenderer) {
						DBGLOG("ngfx", "found %s", kextList[i].id);
						auto method_address = patcher.solveSymbol(index, "__ZN19nvAcceleratorParent18SetAccelPropertiesEv", address, size);
						if (method_address) {
							DBGLOG("ngfx", "obtained __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
							patcher.clearError();
							orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(nvAccelerator_SetAccelProperties), true));
							if (patcher.getError() == KernelPatcher::Error::NoError) {
								DBGLOG("ngfx", "routed __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
							} else {
								SYSLOG("ngfx", "failed to route __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
							}
						} else {
							SYSLOG("ngfx", "failed to resolve __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
						}
					}

					progressState |= ProcessingState::GeForceWebRouted;
				}
				else if (!(progressState & ProcessingState::NVDAStartupWebRouted) && i == KextNVDAStartupWeb)
				{
					DBGLOG("ngfx", "found %s", kextList[i].id);
					auto method_address = patcher.solveSymbol(index, "__ZN14NVDAStartupWeb5probeEP9IOServicePi", address, size);
					if (method_address) {
						DBGLOG("ngfx", "obtained __ZN14NVDAStartupWeb5probeEP9IOServicePi");
						patcher.clearError();
						orgNvdastartupProbe = reinterpret_cast<t_nvdastartup_probe>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(NVDAStartupWeb_probe), true));
						if (patcher.getError() == KernelPatcher::Error::NoError) {
							DBGLOG("ngfx", "routed __ZN14NVDAStartupWeb5probeEP9IOServicePi");
						} else {
							SYSLOG("ngfx", "failed to route __ZN14NVDAStartupWeb5probeEP9IOServicePi");
						}
					} else {
						SYSLOG("ngfx", "failed to resolve __ZN14NVDAStartupWeb5probeEP9IOServicePi");
					}

					progressState |= ProcessingState::NVDAStartupWebRouted;
				}
			}
		}
	}

	// Ignore all the errors for other processors
	patcher.clearError();
}

void NGFX::restoreLegacyOptimisations(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (getKernelVersion() < KernelVersion::HighSierra) {
		DBGLOG("ngfx", "not bothering vaddr presubmit performance fix on pre-10.13");
		return;
	}

	int fifoSubmit = 1;
	PE_parse_boot_argn(Configuration::bootargLegacySubmit, &fifoSubmit, sizeof(fifoSubmit));
	DBGLOG("ngfx", "read legacy fifo submit as %d", fifoSubmit);

	if (!fifoSubmit) {
		DBGLOG("ngfx", "vaddr presubmit performance fix was disabled manually");
		return;
	}

	orgFifoPrepare = patcher.solveSymbol<t_fifo_prepare>(index, "__ZN15nvGpFifoChannel7PrepareEv", address, size);
	if (orgFifoPrepare) {
		DBGLOG("ngfx", "obtained __ZN15nvGpFifoChannel7PrepareEv");
		patcher.clearError();
	} else {
		DBGLOG("ngfx", "failed to resolve __ZN15nvGpFifoChannel7PrepareEv");
	}

	orgFifoComplete = patcher.solveSymbol<t_fifo_complete>(index, "__ZN15nvGpFifoChannel8CompleteEv", address, size);
	if (orgFifoComplete) {
		DBGLOG("ngfx", "obtained __ZN15nvGpFifoChannel8CompleteEv");
		patcher.clearError();
	} else {
		DBGLOG("ngfx", "failed to resolve __ZN15nvGpFifoChannel8CompleteEv");
	}

	if (orgFifoPrepare && orgFifoComplete) {
		mach_vm_address_t presubmitBase = 0;

		// Firstly we need to recover the PreSubmit function, which was badly broken.
		auto presubmit = patcher.solveSymbol(index, "__ZN21nvVirtualAddressSpace9PreSubmitEv", address, size);
		if (presubmit) {
			DBGLOG("ngfx", "obtained __ZN21nvVirtualAddressSpace9PreSubmitEv");
			patcher.clearError();
			// Here we patch the prologue to signal that this call to PreSubmit is not coming from patched areas.
			// The original prologue is executed in orgSubmitHandler, make sure you update it there!!!
			uint8_t prologue[] {0x55, 0x48, 0x89, 0xE5};
			uint8_t uprologue[] {0xB0, 0x00, 0x90, 0x90};
			if (!memcmp(reinterpret_cast<void *>(presubmit), prologue, sizeof(prologue))) {
				patcher.routeBlock(presubmit, uprologue, sizeof(uprologue));
				if (patcher.getError() == KernelPatcher::Error::NoError)
					orgVaddrPresubmitOfficial = reinterpret_cast<t_nvaddr_pre_submit>(
						patcher.routeFunction(presubmit + sizeof(prologue), reinterpret_cast<mach_vm_address_t>(preSubmitHandlerOfficial), true));
				if (patcher.getError() == KernelPatcher::Error::NoError) {
					presubmitBase = presubmit + sizeof(prologue);
					DBGLOG("ngfx", "routed __ZN21nvVirtualAddressSpace9PreSubmitEv");
				} else {
					SYSLOG("ngfx", "failed to route __ZN21nvVirtualAddressSpace9PreSubmitEv");
				}
			} else {
				SYSLOG("ngfx", "prologue mismatch in __ZN21nvVirtualAddressSpace9PreSubmitEv");
			}
		} else {
			SYSLOG("ngfx", "failed to resolve __ZN21nvVirtualAddressSpace9PreSubmitEv");
		}

		// Then we have to recover the calls to the PreSubmit function, which were removed.
		if (orgVaddrPresubmitOfficial && presubmitBase) {
			const char *symbols[] {
				"__ZN21nvVirtualAddressSpace12MapMemoryDmaEP11nvSysMemoryP11nvMemoryMapP18nvPageTableMappingj",
				"__ZN21nvVirtualAddressSpace12MapMemoryDmaEP16__GLNVsurfaceRecjjyj",
				"__ZN21nvVirtualAddressSpace12MapMemoryDmaEyyPK14MMU_MAP_TARGET",
				"__ZN21nvVirtualAddressSpace14UnmapMemoryDmaEP11nvSysMemoryP11nvMemoryMapP18nvPageTableMappingj",
				"__ZN21nvVirtualAddressSpace14UnmapMemoryDmaEP16__GLNVsurfaceRecjjyj",
				"__ZN21nvVirtualAddressSpace14UnmapMemoryDmaEyy"
			};

			uint8_t seq_rbx[] {0xC6, 0x83, 0x7C, 0x03, 0x00, 0x00, 0x00};
			uint8_t seq_r13[] {0x41, 0xC6, 0x85, 0x7C, 0x03, 0x00, 0x00, 0x00};
			uint8_t seq_r12[] {0x41, 0xC6, 0x84, 0x24, 0x7C, 0x03, 0x00, 0x00, 0x00};

			uint8_t rep_rbx[] {0xB0, 0x01, 0xE8, 0x00, 0x00, 0x00, 0x00};
			uint8_t rep_r13[] {0xB0, 0x02, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x90};
			uint8_t rep_r12[] {0xB0, 0x03, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90};

			size_t disp_off = 3;
			// Pick something reasonably high to ensure the sequence is found.
			size_t max_lookup = 0x1000;

			struct {
				uint8_t *patch;
				uint8_t *code;
				size_t sz;
			} patches[] {
				{seq_rbx, rep_rbx, sizeof(seq_rbx)},
				{seq_r13, rep_r13, sizeof(seq_r13)},
				{seq_r12, rep_r12, sizeof(seq_r12)}
			};

			for (auto &sym : symbols) {
				auto addr = patcher.solveSymbol(index, sym, address, size);
				if (addr) {
					DBGLOG("ngfx", "obtained %s", sym);
					patcher.clearError();

					for (size_t off = 0; off < max_lookup; off++) {
						for (auto &patch : patches) {
							if (!memcmp(reinterpret_cast<uint8_t *>(addr+off), patch.patch, patch.sz)) {
								// Calculate the jump offset
								auto disp = static_cast<int32_t>(presubmitBase - (addr+off+disp_off + 5));
								DBGLOG("ngfx", "found pattern of %lu bytes at %lu offset, disp %X", patch.sz, off, disp);
								*reinterpret_cast<int32_t *>(patch.code + disp_off) = disp;
								patcher.routeBlock(addr+off, patch.code, patch.sz);
								if (patcher.getError() == KernelPatcher::Error::NoError)
									DBGLOG("ngfx", "successfully patched %s", sym);
								else
									SYSLOG("ngfx", "failed to patch %s", sym);
								off = max_lookup;
								break;
							}
						}
					}

				} else {
					SYSLOG("ngfx", "failed to obtain %s", sym);
				}
			}
		}
	}
}

void NGFX::nvAccelerator_SetAccelProperties(IOService* that)
{
	DBGLOG("ngfx", "SetAccelProperties is called");

	if (callbackNGFX && callbackNGFX->orgSetAccelProperties)
	{
		callbackNGFX->orgSetAccelProperties(that);

		if (!that->getProperty("IOVARendererID"))
		{
			uint8_t rendererId[] {0x08, 0x00, 0x04, 0x01};
			that->setProperty("IOVARendererID", rendererId, sizeof(rendererId));
			DBGLOG("ngfx", "set IOVARendererID to 08 00 04 01");
		}

		if (!that->getProperty("IOVARendererSubID"))
		{
			uint8_t rendererSubId[] {0x03, 0x00, 0x00, 0x00};
			that->setProperty("IOVARendererSubID", rendererSubId, sizeof(rendererSubId));
			DBGLOG("ngfx", "set IOVARendererSubID to value 03 00 00 00");
		}
	}

	auto gfx = that->getParentEntry(gIOServicePlane);
	int gl = gfx && gfx->getProperty("disable-metal");
	PE_parse_boot_argn(Configuration::bootargForceOpenGL, &gl, sizeof(gl));

	if (gl) {
		DBGLOG("ngfx", "disabling metal support");
		that->removeProperty("MetalPluginClassName");
		that->removeProperty("MetalPluginName");
		that->removeProperty("MetalStatisticsName");
	}
}

bool NGFX::AppleGraphicsDevicePolicy_start(IOService *that, IOService *provider)
{
	bool result = false;

	DBGLOG("ngfx", "AppleGraphicsDevicePolicy::start is called");    
	if (callbackNGFX && callbackNGFX->orgAgdpStart)
	{
		char board_id[32];
		if (WIOKit::getComputerInfo(nullptr, 0, board_id, sizeof(board_id)))
		{
			DBGLOG("ngfx", "got board-id '%s'", board_id);
			auto dict = that->getPropertyTable();
			auto newProps = OSDynamicCast(OSDictionary, dict->copyCollection());
			OSDictionary *configMap = OSDynamicCast(OSDictionary, newProps->getObject("ConfigMap"));
			if (configMap != nullptr)
			{
				OSString *value = OSDynamicCast(OSString, configMap->getObject(board_id));
				if (value != nullptr)
					DBGLOG("ngfx", "Current value for board-id '%s' is %s", board_id, value->getCStringNoCopy());
				if (!configMap->setObject(board_id, OSString::withCString("none")))
					SYSLOG("ngfx", "Configuration for board-id '%s' can't be set, setObject was failed.", board_id);
				else
					DBGLOG("ngfx", "Configuration for board-id '%s' has been set to none", board_id);
				that->setPropertyTable(newProps);
			}
			else
			{
				SYSLOG("ngfx", "ConfigMap key was not found in personalities");
				OSSafeReleaseNULL(newProps);
			}
		}

		result = callbackNGFX->orgAgdpStart(that, provider);
		DBGLOG("ngfx", "AppleGraphicsDevicePolicy::start returned %d", result);
	}

	return result;
}

IOService* NGFX::NVDAStartupWeb_probe(IOService *that, IOService * provider, SInt32 *score)
{
	IOService* result = nullptr;

	DBGLOG("ngfx", "NVDAStartupWeb::probe is called");
	if (callbackNGFX && callbackNGFX->orgNvdastartupProbe)
	{
		int comp = config.force_compatibility;
		if (comp < 0)
			comp = provider && provider->getProperty("force-compat");

		if (comp > 0) {
			char osversion[40] = {};
			size_t size = sizeof(osversion);
			sysctlbyname("kern.osversion", osversion, &size, NULL, 0);
			DBGLOG("ngfx", "ignoring driver compatibility requirements with %s OS", osversion);
			that->setProperty("NVDARequiredOS", osversion);
		}

		result = callbackNGFX->orgNvdastartupProbe(that, provider, score);
	}

	return result;
}

int NGFX::csfg_get_platform_binary(void *fg)
{
	//DBGLOG("ngfx", "csfg_get_platform_binary is called"); // is called quite often

	if (callbackNGFX && callbackNGFX->org_csfg_get_platform_binary && callbackNGFX->csfg_get_teamid)
	{
		int result = callbackNGFX->org_csfg_get_platform_binary(fg);
		if (!result)
		{
			// Special case NVIDIA drivers
			const char *teamId = callbackNGFX->csfg_get_teamid(fg);
			if (teamId != nullptr && strcmp(teamId, kNvidiaTeamId) == 0)
			{
				DBGLOG("ngfx", "platform binary override for %s", kNvidiaTeamId);
				return 1;
			}
		}

		return result;
	}

	// Default to error
	return 0;
}

bool NGFX::nvVirtualAddressSpace_PreSubmitOfficial(void *that)
{
	if (callbackNGFX && orgVaddrPresubmitOfficial)
	{
		bool r = orgSubmitHandlerOfficial(that);

		if (that && r && callbackNGFX->orgFifoPrepare && callbackNGFX->orgFifoComplete)
		{
			getMember<uint8_t>(that, 0x37D) = 1;
			auto fifo = getMember<void *>(that, 0x2B0);
			if (callbackNGFX->orgFifoPrepare(fifo))
			{
				auto fifovt = getMember<void *>(fifo, 0);
				// Calls to nvGpFifoChannel::PreSubmit
				auto fifopresubmit = getMember<bool (*)(void *, uint32_t, void *, uint32_t,
					void *, uint32_t *, uint64_t, uint32_t)>(fifovt, 0x1B0);
				if (fifopresubmit(fifo, 0x40000, 0, 0, 0, 0, 0, 0))
				{
					getMember<uint16_t>(that, 0x37C) = 1;
					return true;
				}

				callbackNGFX->orgFifoPrepare(fifo);
				return false;
			}
		}

		return r;
	}

	return false;
}

void NGFX::applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchNum, const char* name) {
	DBGLOG("ngfx", "applying patch '%s' for %zu kext", name, index);
	for (size_t p = 0; p < patchNum; p++) {
		auto &patch = patches[p];
		if (patch.patch.kext->loadIndex == index) {
			if (patcher.compatibleKernel(patch.minKernel, patch.maxKernel)) {
				DBGLOG("ngfx", "applying %zu patch for %zu kext", p, index);
				patcher.applyLookupPatch(&patch.patch);
				// Do not really care for the errors for now
				patcher.clearError();
			}
		}
	}
}

