#ifndef DC_HYDRO_PEER_LAYOUT_H
#define DC_HYDRO_PEER_LAYOUT_H

/* The upper half of the simulation control buffer is live only during paired rigid work. */
#define DC_HYDRO_PEER_WIDTH 16u
#define DC_HYDRO_PEER_HEIGHT 17u
#define DC_HYDRO_PEER_OFFSET 18u
#define DC_HYDRO_PEER_ADDRESSES 20u
#define DC_HYDRO_PAGES 0u
#define DC_HYDRO_CELLS 1u
#define DC_HYDRO_OCCUPANCY 2u
#define DC_HYDRO_VELOCITY 3u
#define DC_HYDRO_BULK 4u
#define DC_HYDRO_BUFFERS 5u
#define DC_HYDRO_LOCAL_BULK_ADDRESS 30u
#endif
