#ifndef DC_HYDRO_PEER_LAYOUT_H
#define DC_HYDRO_PEER_LAYOUT_H

/* Control words from 16 onward are live only during paired rigid work.
 * Words 30 and 31 retain the local bulk address between the peer fields. */
#define DC_HYDRO_PEER_RASTER 1024u
#define DC_HYDRO_MPM_OCCUPANCY_PLANE 5u
#define DC_HYDRO_PEER_WIDTH 16u
#define DC_HYDRO_PEER_HEIGHT 17u
#define DC_HYDRO_PEER_OFFSET 18u
#define DC_HYDRO_PEER_ADDRESSES 20u
#define DC_HYDRO_PAGES 0u
#define DC_HYDRO_CELLS 1u
#define DC_HYDRO_OCCUPANCY 2u
#define DC_HYDRO_VELOCITY 3u
#define DC_HYDRO_BULK 4u
#define DC_HYDRO_PARTICLES 5u
#define DC_HYDRO_GRAIN_SCRATCH 6u
#define DC_HYDRO_BUFFERS 7u
#define DC_HYDRO_LOCAL_BULK_ADDRESS 30u
#define DC_HYDRO_ADDRESS_WORD(field) (DC_HYDRO_PEER_ADDRESSES + 2u*(field) + ((field) >= DC_HYDRO_PARTICLES ? 2u : 0u))
#define DC_HYDRO_CONTROL_BYTES (4u * DC_HYDRO_ADDRESS_WORD(DC_HYDRO_BUFFERS))
#define DC_HYDRO_PEER_WORDS (DC_HYDRO_ADDRESS_WORD(DC_HYDRO_BUFFERS) - DC_HYDRO_PEER_WIDTH)
#endif
