#ifndef LIBPRESS_H
#define LIBPRESS_H

#include <types.h>

/* EncSPU constants */
#define ENCSPU_ENCODE_LOOP 1           /**< Generate loop waveform data */
#define ENCSPU_ENCODE_NO_LOOP 0        /**< Generate non-loop waveform data */
#define ENCSPU_ENCODE_ENDIAN_BIG 0     /**< 16-bit big endian */
#define ENCSPU_ENCODE_ENDIAN_LITTLE 1  /**< 16-bit little endian */
#define ENCSPU_ENCODE_WHOLE 0          /**< Whole encoding */
#define ENCSPU_ENCODE_START 1          /**< Start divided encoding */
#define ENCSPU_ENCODE_CONTINUE 2       /**< Continue divided encoding */
#define ENCSPU_ENCODE_END 3            /**< End divided encoding */
#define ENCSPU_ENCODE_MIDDLE_QUALITY 0 /**< Middle quality encoding */
#define ENCSPU_ENCODE_HIGH_QUALITY 1   /**< High quality encoding */

/**
 * @brief Quantization tables used during MDEC decoding process
 *
 * This structure contains the tables used during the reverse-quantization step
 * of the MDEC decoding process.
 */
typedef struct {
    u_char iq_y[64]; /**< Brightness component quantization table */
    u_char iq_c[64]; /**< Chrominance component quantization table */
    short dct[64];   /**< System reserved */
} DECDCTENV;

/**
 * @brief SPU encode environment attribute structure
 *
 * This structure is used to specify the SPU encode environment attributes for
 * EncSPU() function. When ENCSPU_ENCODE_NO_LOOP is specified for loop,
 * loop_start is ignored.
 */
typedef struct {
    short* src;      /**< 16-bit PCM data address */
    short* dest;     /**< PlayStation original waveform data */
    short* work;     /**< Work area when encode processing */
    long size;       /**< 16-bit PCM data size (in bytes) */
    long loop_start; /**< PCM data loop start point (in bytes) */
    char loop;       /**< Loop waveform generation specification */
    char byte_swap;  /**< PCM data endian specification */
    char proceed;    /**< Whole/Divided encoding specification */
    char pad4;       /**< System reserved */
} ENCSPUENV;

/**
 * @brief Get size of run-level DCT data
 *
 * Returns the uncompressed length of the data contained in the Huffman-encoded
 * bitstream pointed to by the bs parameter. It does not perform the actual
 * decoding.
 *
 * @param bs Pointer to bitstream
 * @return Length of uncompressed data in long words
 */
long DecDCTBufSize(u_long* bs);

/**
 * @brief Get current quantization tables and environment data used during MDEC
 * image decoding
 *
 * Returns the current decoding environment to env.
 *
 * @param env Pointer to decoding environment
 * @return Address of env
 */
DECDCTENV* DecDCTGetEnv(DECDCTENV* env);

/**
 * @brief Begin decoding RLE-encoded MDEC image data
 *
 * Begins decoding the RLE-encoded MDEC image data at the address specified by
 * runlevel. A maximum of 128k may be decoded at a time.
 *
 * @param runlevel Pointer to input runlevel
 * @param mode Decode mode (bit 0: 0=16-bit, 1=24-bit; bit 1: STP bit in 16-bit
 * mode)
 */
void DecDCTin(unsigned long* runlevel, long mode);

/**
 * @brief Install a callback routine to be called at termination of MDEC
 * transmission
 *
 * Registers a callback function to be called when MDEC transmission completes.
 *
 * @param func Callback function pointer
 * @return Previously installed callback function
 */
void (*DecDCTinCallback(void (*func)()))(void);

/**
 * @brief Check MDEC transmission status
 *
 * Checks the status of MDEC transmission.
 *
 * @param mode 0 = wait for completion, 1 = check status
 * @return Status value
 */
int DecDCTinSync(int mode);

/**
 * @brief Begin retrieving decoded MDEC image data
 *
 * Begins retrieving decoded MDEC image data to the address specified by buf.
 *
 * @param buf Pointer to output buffer
 * @param size Size of data to retrieve (in long words)
 */
void DecDCTout(unsigned long* buf, long size);

/**
 * @brief Install a callback routine to be called at completion of MDEC
 * reception
 *
 * Registers a callback function to be called when MDEC reception completes.
 *
 * @param func Callback function pointer
 * @return Previously installed callback function
 */
void (*DecDCToutCallback(void (*func)()))(void);

/**
 * @brief Check MDEC reception status
 *
 * Checks the status of MDEC reception.
 *
 * @param mode 0 = wait for completion, 1 = check status
 * @return Status value
 */
int DecDCToutSync(int mode);

/**
 * @brief Set quantization tables used during MDEC image decoding
 *
 * Sets the quantization tables used during MDEC decoding.
 *
 * @param env Pointer to decoding environment
 */
void DecDCTPutEnv(DECDCTENV* env);

/**
 * @brief Reset MDEC decoder
 *
 * Resets the MDEC decoder to its initial state.
 *
 * @param mode Reset mode
 */
void DecDCTReset(long mode);

/**
 * @brief Decode Huffman-encoded bitstream
 *
 * Decodes a Huffman-encoded bitstream into run-level data.
 *
 * @param bs Pointer to bitstream
 * @param buf Pointer to output buffer
 * @return Number of long words decoded
 */
int DecDCTvlc(u_long* bs, u_long* buf);

/**
 * @brief Decode Huffman-encoded bitstream (alternate version)
 *
 * Alternate version of DecDCTvlc().
 *
 * @param bs Pointer to bitstream
 * @param buf Pointer to output buffer
 * @return Number of long words decoded
 */
int DecDCTvlc2(u_long* bs, u_long* buf);

/**
 * @brief Build VLC table
 *
 * Builds a variable-length code table.
 *
 * @param table Pointer to table buffer
 * @return Status value
 */
void DecDCTvlcBuild(u_short* table);

/**
 * @brief Get size of VLC table
 *
 * Returns the size required for a VLC table.
 *
 * @return Size in long words
 */
long DecDCTvlcSize(void);

/**
 * @brief Get size of VLC table (alternate version)
 *
 * Alternate version of DecDCTvlcSize().
 *
 * @return Size in long words
 */
long DecDCTvlcSize2(void);

/**
 * @brief Encode 16-bit PCM data to SPU ADPCM format
 *
 * Converts 16-bit PCM data to PlayStation SPU ADPCM format.
 *
 * @param env Pointer to encoding environment
 * @return Status value
 */
long EncSPU(ENCSPUENV* env);

/**
 * @brief Encode 16-bit PCM data to SPU ADPCM format (high quality)
 *
 * High-quality version of EncSPU().
 *
 * @param env Pointer to encoding environment
 * @return Status value
 */
long EncSPU2(ENCSPUENV* env);

#endif
