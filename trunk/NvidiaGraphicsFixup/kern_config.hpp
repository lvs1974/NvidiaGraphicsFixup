//
//  kern_config.hpp
//  NvidiaGraphicFixup
//
//  Copyright © 2016-2017 lvs1974. All rights reserved.
//

#ifndef kern_config_private_h
#define kern_config_private_h


class Configuration {
public:
	/**
	 *  Possible boot arguments
	 */
    static const char *bootargOff[];
    static const char *bootargDebug[];
    static const char *bootargBeta[];
    static constexpr const char *bootargNoAudio    {"-ngfxnoaudio"};    // disable all audio fixes
    static constexpr const char *bootargNoAudioCon {"-ngfxnoaudiocon"}; // disable adding of @0,connector-type - @5,connector-type
    static constexpr const char *bootargSetConfig  {"ngfxsetcfg"};      // set config for board-id in ConfigMap of AppleGraphicsDevicePolicy {none, Config1, Config2, Config3}
    static constexpr const char *bootargPatchList  {"ngfxpatchlist"};   // list of patches: pikera,vit9696 (by default)

    
public:
	/**
	 *  Retrieve boot arguments
	 */
	void readArguments();
	
    /**
     *  disable all audio fixes
     */
    bool noaudiofixes {false};
    
	/**
	 *  disable adding of @0,connector-type - @5,connector-type
	 */
    bool noaudioconnectors {false};
    
    /**
     *  name of config used by AppleGraphicsDevicePolicy {none, Config1, Config2, Config3}
     */
    char apgdp_config_name[15] {};
    
    /**
     *  patch list (can be separated by comma, space or something like that)
     */
    char patch_list[64] = {};

    Configuration() = default;
};

extern Configuration config;

#endif /* kern_config_private_h */
