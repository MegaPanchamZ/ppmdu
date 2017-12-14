#ifndef BPC_HPP
#define BPC_HPP
/*
bpc.hpp
2016/09/28
psycommando@gmail.com
Description: Utilities for handling the BPC file format.
*/
#include <ppmdu/pmd2/pmd2.hpp>
#include <utils/gbyteutils.hpp>
#include <ppmdu/containers/level_tileset.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace filetypes
{
//============================================================================================
//  Constants
//============================================================================================
    const std::string BPC_FileExt = "bpc";


//============================================================================================
//  Struct
//============================================================================================
    /*
        bpc_header
            The header of a BPC file
    */
    struct bpc_header
    {
        static const size_t LEN        = 28; //bytes
        static const size_t NbTilesets = 2;
        struct indexentry
        {
            uint16_t nbtiles;   //The nb of 64 pixels, 4bpp tiles in the image
            uint16_t unk2;
            uint16_t unk3;
            uint16_t unk4;
            uint16_t unk5;
            uint16_t tmapdeclen; //Decompressed length of the compressed tile mapping table right after the image

            template<class _outit>
                _outit Write( _outit itw )const
            {
                itw = utils::WriteIntToBytes(nbtiles,       itw );
                itw = utils::WriteIntToBytes(unk2,          itw );
                itw = utils::WriteIntToBytes(unk3,          itw );
                itw = utils::WriteIntToBytes(unk4,          itw );
                itw = utils::WriteIntToBytes(unk5,          itw );
                itw = utils::WriteIntToBytes(tmapdeclen,    itw );
                return itw;
            }

            template<class _fwdinit>
                _fwdinit Read( _fwdinit itr, _fwdinit itpend )
            {
                itr = utils::ReadIntFromBytes(nbtiles,      itr, itpend );
                itr = utils::ReadIntFromBytes(unk2,         itr, itpend );
                itr = utils::ReadIntFromBytes(unk3,         itr, itpend );
                itr = utils::ReadIntFromBytes(unk4,         itr, itpend );
                itr = utils::ReadIntFromBytes(unk5,         itr, itpend );
                itr = utils::ReadIntFromBytes(tmapdeclen,   itr, itpend );
                return itr;
            }
        };
        uint16_t                            offsuprscr;
        uint16_t                            offslowrscr;
        std::array<indexentry,NbTilesets>   tilesetsinfo;

        template<class _outit>
            _outit Write( _outit itw )const
        {
            itw = utils::WriteIntToBytes(offsuprscr,    itw );
            itw = utils::WriteIntToBytes(offslowrscr,   itw );
            for( const auto & entry : tilesetsinfo )
                itw = entry.Write(itw);
            return itw;
        }

        template<class _fwdinit>
            _fwdinit Read( _fwdinit itr, _fwdinit itpend )
        {
            itr = utils::ReadIntFromBytes(offsuprscr,    itr, itpend );
            itr = utils::ReadIntFromBytes(offslowrscr,   itr, itpend );
            for( auto & entry : tilesetsinfo )
                itr = entry.Read(itr,itpend);
            return itr;
        }
    };

//============================================================================================
//  Functions
//============================================================================================
    std::pair<pmd2::Tileset,pmd2::Tileset>  ParseBPC( const std::string & fpath );
    void                                    WriteBPC( const std::string & destfpath, const pmd2::Tileset & srcupscr, const pmd2::Tileset & srcbotscr );

};

#endif