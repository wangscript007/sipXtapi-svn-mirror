//
// Copyright (C) 2005-2007 SIPfoundry Inc.
// License by SIPfoundry under the LGPL license.
// 
// Copyright (C) 2004 Pingtel Corp.
// Licensed to SIPfoundry under a Contributor Agreement.
//
// Copyright (C) 2007 SIPez, LLC.
// Licensed to SIPfoundry under a Contributor Agreement.
//
////////////////////////////////////////////////////////////////////////

#ifndef _NetBase64Codec_h_
#define _NetBase64Codec_h_

// SYSTEM INCLUDES

// APPLICATION INCLUDES
#include "utl/UtlString.h"

// DEFINES
// MACROS
// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS


/// Provides methods for translating to and from base64 encoding.
/**
 * Base 64 is a convenient encoding used to translate arbitrary binary
 * data into a fixed 64 character subset of ascii (plus one additional
 * character used to indicate padding).  This implementation* uses the
 * alphabet specified in Table 1 of RFC 3548 (which is the standard MIME
 * alphabet).
 *
 * @nosubgrouping
 */
class NetBase64Codec
{
/* //////////////////////////// PUBLIC //////////////////////////////////// */
  public:

/* ============================ CREATORS ================================== */

   // ================================================================
   /** @name                  Encoding Operations
    *
    * These methods translate from the binary data to the encodedData string
    */
   ///@{

   /// Encode from one array into another
   static void encode(int dataSize,         ///< the size of the binary data in octets         
                      const char data[],    ///< the binary data - not null terminated         
                      int& encodedDataSize, ///< output: the size of the encoded data in octets
                      char encodedData[]    ///< output: the encoded data                      
                      );

   /// Encode from an array into a UtlString
   static void encode(int dataSize,          ///< the size of the binary data in octets
                      const char data[],     ///< the binary data - not null terminated
                      UtlString& encodedData ///< output: the encoded data
                      );

   /// Encode from one UtlString into another.
   static void encode(const UtlString& data, ///< size is data.length(), not null terminated
                      UtlString& encodedData ///< output: the encoded data
                      )
   {
      NetBase64Codec::encode(data.length(),data.data(),encodedData);
   };
   
   /// @returns the number of encoded octets for given number of input binary octets
   static int encodedSize(int dataSize);

   ///@}
   
   // ================================================================
   /** @name                  Decoding Operations
    *
    * The decoding methods translate from the encoded parameter to the binary data
    * All return false if the encoded data value contained any characters
    * that are not legal in the base64 alphabet.
    */
   ///@{

   /// @returns true iff the encoded data is syntactically valid.
   static bool isValid(int encodedDataSize,      ///< the size of the encoded data in octets
                       const char encodedData[]  ///< the encoded data 
                       )
   {
      return validEncodingBytes(encodedDataSize, encodedData) > 0;
   }
   

   /// @returns true iff the encoded data is syntactically valid.
   static bool isValid(const UtlString& encodedData ///< size is data.length(), not null terminated
                       )
   {
      return validEncodingBytes(encodedData.length(), encodedData.data()) > 0;
   }
   
   /// Decode from the character encodedData to the binary data array.
   static bool decode(int encodedDataSize,      ///< the size of the encoded data in octets
                      const char encodedData[], ///< the encoded data 
                      int& dataSize,            ///< output: the size of the binary data in octets
                      char data[]               ///< output: the binary data - not null terminated
                      );
   ///< @returns false and no data if the encodedData contains any invalid characters.

   /// Decode from one UtlString into another
   static bool decode(const UtlString& encodedData, ///< size is data.length(), not null terminated
                      UtlString& data               ///< output: the decoded data
                      );
   ///< @returns false and no data if the encodedData contains any invalid characters.
   
   /// Compute the number of output binary octets for given set of encoded octets.
   static int decodedSize(int encodedDataSize,
                          const char encodedData[]
                          );
   ///< @returns zero if the encodedData contains any invalid characters.

   /// Compute the number of output binary octets for given set of encoded octets.
   static int decodedSize(const UtlString& encodedData  ///< size is data.length()
                          )
   {
      ///< @returns zero if the encodedData contains any invalid characters.
      return decodedSize(encodedData.length(), encodedData.data());
   }
   

  private:
   
   static const char* Base64Codes;

   inline static char decodeChar(const char encoded);

   /// @returns > 0 iff the encoded data is syntactically valid, 0 if not.
   static size_t validEncodingBytes(int encodedDataSize, ///< number of encoded octets
                                    const char encodedData[]  ///< the encoded data 
                                    );

   ///@}

   // @cond INCLUDENOCOPY
   NetBase64Codec();
   //:Default constructor (disabled)


   virtual
      ~NetBase64Codec();
   
   NetBase64Codec(const NetBase64Codec& rNetBase64Codec);
   //:Copy constructor (disabled)

   NetBase64Codec& operator=(const NetBase64Codec& rhs);
   //:Assignment operator (disabled)

   // @endcond     
};

#endif  // _NetBase64Codec_h_
