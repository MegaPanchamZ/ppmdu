#ifndef SMDL_HPP
#define SMDL_HPP
/*
smdl.hpp
2015/05/20
psycommando@gmail.com
Description: Utilities for handling Pokemon Mystery Dungeon: Explorers of Sky/Time/Darkness's .smd files.
*/
#include <ppmdu/fmts/dse_common.hpp>

namespace DSE
{

//====================================================================================================
//  Typedefs / Enums
//====================================================================================================

    //Nb of track events in the event enum
    static const uint32_t NB_Track_Events = 16;

    /************************************************************************
        eTrkEventCodes
            Codes for each different events stored in a smdl track chunk.
    ************************************************************************/
    enum struct eTrkEventCodes : uint8_t
    {
        Invalid       = 0x00,
        NoteOnBeg     = 0x01, //The first event code reserved for play note events.
        NoteOnEnd     = 0x7F, //The last event code that is reserved for playing a note.

        NoteDTBeg     = 0x80, //The first value reserved for note Delta-Time
        NoteDTEnd     = 0x8F, //The last value reserved for note Delta-Time

        RepeatSilence = 0x90, //Repeat the last silence
        Silence       = 0x92, //Silence the track for specified duration (uses a uint8)
        LongSilence   = 0x93, //Silence the track for specified duration (uses a uint16)
        EndOfTrack    = 0x98, //Marks the end of the track. Also serve as padding.
        LoopPointSet  = 0x99, //Marks the location where the track should loop from.
        SetOctave     = 0xA0, //Sets the octave notes are currently played at.
        SetTempo      = 0xA4, //Sets the tempo of the track in BPM.
        SetUnk1       = 0xA9, //Set that first unknown value from the track's header
        SetUnk2       = 0xAA, //Set that second unknown value from the track's header
        SetPreset     = 0xAC, //Sets the instrument preset to use
        Modulate      = 0xD7, //Pitch bending/modulation/LFO. Not 100% certain.
        SetUnk3       = 0xDB, //Unknown purpose. Used in bgmM0000.smd
        SetTrkVol     = 0xE0, //Sets primary track volume.
        SetExpress    = 0xE3, //Sets secondary volume control. AKA expression or GM CC#11.
        SetTrkPan     = 0xE8, //Sets the panning of the track.
    };

//====================================================================================================
// Structs
//====================================================================================================
    
    /************************************************************************
        TrkEventInfo
            Contains details specifics on how to parse an event codes.
    ************************************************************************/
    struct TrkEventInfo
    {
        //Event code range
        eTrkEventCodes evcodebeg;   //= eTrkEventCodes::Invalid; //Beginning of the range of event codes that can be used to represent this event.
        eTrkEventCodes evcodeend;   //= eTrkEventCodes::Invalid; //Leave to invalid when is event with single code

        //nb params
        uint32_t       nbreqparams; //= 0;     //if has any required parameters
        bool           hasoptparam; //= false; //if has optional param 
        bool           isEoT;       //= false; //if is end of track marker
        bool           isLoopPoint; //= false; //if is loop point
    };

    ///************************************************************************
    //    TrkEvent
    //        Represent a raw track event.
    //************************************************************************/
    //struct TrkEvent
    //{
    //    uint8_t dt     = 0;
    //    uint8_t evcode = 0;
    //    uint8_t param1 = 0;
    //    uint8_t param2 = 0;
    //};

    /************************************************************************
        SongChunk
            The raw song chunk.
            For some reasons, most of the data in this chunk rarely ever 
            changes in-between games or files.. Only the nb of channels and
            tracks does..
    ************************************************************************/
    struct SongChunk
    {
        static const uint32_t SizeNoPadd    = 48; //bytes
        static const uint32_t LenMaxPadding = 16; //bytes

        unsigned int size()const { return SizeNoPadd + unkpad.size(); }

        uint32_t label   = 0;
        uint32_t unk1    = 0;
        uint32_t unk2    = 0;
        uint32_t unk3    = 0;
        uint32_t unk4    = 0;
        uint16_t unk5    = 0;
        uint8_t  nbtrks  = 0;
        uint8_t  nbchans = 0;
        uint32_t unk6    = 0;
        uint32_t unk7    = 0;
        uint32_t unk8    = 0;
        uint32_t unk9    = 0;
        uint16_t unk10   = 0;
        uint16_t unk11   = 0;
        uint32_t unk12   = 0;
        std::vector<uint8_t> unkpad;

        //
        template<class _outit>
            _outit WriteToContainer( _outit itwriteto )const
        {
            itwriteto = utils::WriteIntToByteVector( static_cast<uint32_t>(eDSEChunks::song), itwriteto, false ); //Force this, to avoid bad surprises
            itwriteto = utils::WriteIntToByteVector( unk1,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk2,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk3,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk4,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk5,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( nbtrks,  itwriteto );
            itwriteto = utils::WriteIntToByteVector( nbchans, itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk6,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk7,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk8,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk9,    itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk10,   itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk11,   itwriteto );
            itwriteto = utils::WriteIntToByteVector( unk12,   itwriteto );
            itwriteto = std::copy( unkpad.begin(), unkpad.end(), itwriteto );
            return itwriteto;
        }

        //
        template<class _init>
            _init ReadFromContainer(  _init itReadfrom )
        {
            label   = utils::ReadIntFromByteVector<decltype(label)>  (itReadfrom, false ); //iterator is incremented
            unk1    = utils::ReadIntFromByteVector<decltype(unk1)>   (itReadfrom); 
            unk2    = utils::ReadIntFromByteVector<decltype(unk2)>   (itReadfrom);
            unk3    = utils::ReadIntFromByteVector<decltype(unk3)>   (itReadfrom);
            unk4    = utils::ReadIntFromByteVector<decltype(unk4)>   (itReadfrom);
            unk5    = utils::ReadIntFromByteVector<decltype(unk5)>   (itReadfrom);
            nbtrks  = utils::ReadIntFromByteVector<decltype(nbtrks)> (itReadfrom);
            nbchans = utils::ReadIntFromByteVector<decltype(nbchans)>(itReadfrom);
            unk6    = utils::ReadIntFromByteVector<decltype(unk6)>   (itReadfrom);
            unk7    = utils::ReadIntFromByteVector<decltype(unk7)>   (itReadfrom);
            unk8    = utils::ReadIntFromByteVector<decltype(unk8)>   (itReadfrom);
            unk9    = utils::ReadIntFromByteVector<decltype(unk9)>   (itReadfrom);
            unk10   = utils::ReadIntFromByteVector<decltype(unk10)>  (itReadfrom);
            unk11   = utils::ReadIntFromByteVector<decltype(unk11)>  (itReadfrom);
            unk12   = utils::ReadIntFromByteVector<decltype(unk12)>  (itReadfrom);

            for( uint32_t i = 0; i < LenMaxPadding; ++i, ++itReadfrom )
            {
                if( *itReadfrom == 0xFF )
                    unkpad.push_back( 0xFF ); //save on dereferencing the iterator..
                else
                    break;
            }

            return itReadfrom;
        }
    };

    /*
    */


//====================================================================================================
// Class
//====================================================================================================

    /************************************************************************
        EvTrack
            A track made out of raw events.
    ************************************************************************/
    class EvTrack
    {
    public:
        typedef std::vector<TrkEvent>   track_t;
        typedef track_t::iterator       ittrk_t;
        typedef track_t::const_iterator cittrk_t;

        EvTrack() 
        {}

        // *** Events ops ***
        void             pushback( TrkEvent && ev )      { return m_events.push_back(ev); }
        TrkEvent      && popback ()
        {
            TrkEvent tmp;
            if( !m_events.empty() )
            {
                tmp = std::move( m_events[m_events.size()-1] );
                m_events.pop_back();
            }
            else
                throw std::runtime_error("EvTrack: Event track is empty! Cannot popback!");
            return std::move(tmp);
        }

        ittrk_t          begin()                         { return m_events.begin(); }
        cittrk_t         begin()const                    { return m_events.begin(); }
        ittrk_t          end()                           { return m_events.end();   }
        cittrk_t         end()const                      { return m_events.end();   }

        TrkEvent       & operator[]( size_t index )      { return m_events[index];  }
        const TrkEvent & operator[]( size_t index )const { return m_events[index];  }

        size_t size   ()const                            { return m_events.size();  }
        void   resize ( size_t newsz )                   { m_events.resize(newsz);  }
        void   reserve( size_t ressz )                   { m_events.reserve(ressz); }

    private:
        track_t m_events;
    };


//====================================================================================================
// Constants
//====================================================================================================

    /************************************************************************
        TrkEventsTable
            Contains details specifics on how to parse all event codes.
    ************************************************************************/
    static const std::array<TrkEventInfo, NB_Track_Events> TrkEventsTable;

//====================================================================================================
// Functions
//====================================================================================================

    pmd2::audio::MusicSequence ParseSMDL( const std::string & file );
    void                       WriteSMDL( const std::string & file, const pmd2::audio::MusicSequence & seq );

};

#endif