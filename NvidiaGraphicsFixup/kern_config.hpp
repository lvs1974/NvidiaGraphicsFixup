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
    static constexpr const char *bootargNoAudio      {"-ngfxnoaudio"};        // disable all audio fixes
    static constexpr const char *bootargNoAudioCon   {"-ngfxnoaudiocon"};     // disable adding of @0,connector-type - @5,connector-type
    static constexpr const char *bootargNoVARenderer {"-ngfxnovarenderer"};   // disable IOVARenderer injection
    static constexpr const char *bootargNoLibValFix  {"-ngfxlibvalfix"};      // disable NVWebDriverLibValFix fix
    static constexpr const char *bootargPatchList    {"ngfxpatch"};           // comma separated patches: cfgmap,pikera,vit9696 (by default)
    static constexpr const char *bootargCompat       {"ngfxcompat"};          // force compatibility

    
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
     *  disable IOVARenderer injection
     */
    bool novarenderer {false};
    
    /**
     *  disable NVWebDriverLibValFix fix
     */
    bool nolibvalfix {false};
    
    /**
     *  patch list (can be separated by comma, space or something like that)
     */
    char patch_list[64] {"vit9696"};
	
	/**
	 *  ignore compatibility check in driver
	 */
	int force_compatibility {-1};

    Configuration() = default;
};

extern Configuration config;

#endif /* kern_config_private_h */
