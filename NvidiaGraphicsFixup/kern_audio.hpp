//
//  kern_audio.hpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#ifndef kern_audio_hpp
#define kern_audio_hpp

#include <Headers/kern_util.hpp>

#include <Library/LegacyIOService.h>
#include <sys/types.h>



class EXPORT NVidiaAudio : public IOService {
	OSDeclareDefaultStructors(NVidiaAudio)
	uint32_t getAnalogLayout();
public:
	IOService *probe(IOService *provider, SInt32 *score) override;
    
    struct VendorID {
        enum : uint16_t {
            ATIAMD = 0x1002,
            NVIDIA = 0x10de,
            Intel = 0x8086
        };
    };
    
    static constexpr int MaxConnectorCount = 6;
};

#endif /* kern_audio_hpp */
