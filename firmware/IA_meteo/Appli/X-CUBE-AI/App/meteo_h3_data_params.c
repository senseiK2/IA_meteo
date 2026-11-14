/**
  ******************************************************************************
  * @file    meteo_h3_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-14T12:21:55+0100
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "meteo_h3_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_meteo_h3_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_meteo_h3_weights_array_u64[242] = {
  0xbf348a1b3f320093U, 0xbe3b210b3e283ebcU, 0xbe417ef5be77c14aU, 0x3da1787f3d3f9dd5U,
  0xbee05c5a3ef42b9bU, 0x3d9b614cbe041d99U, 0x3f237f58be2350a5U, 0x3f3010723e1e2883U,
  0x3df1654b3d76753eU, 0xbe1ea521be1749d0U, 0xbf379355bbf5eb02U, 0xbe90e8d33e6b24d8U,
  0xbe8b7fb93ea0d331U, 0xbe372653bf03fd2aU, 0x3e0312c4bedd1fc5U, 0xbd71622dbea2cea3U,
  0xbf1210583e703574U, 0xbef71daabd417520U, 0x3e8360fd3ef9cfb9U, 0x3f44cc4bbe020443U,
  0x3e55b4193e945489U, 0xbe9fd062be63cbf9U, 0xbf1cebb43ef9eee0U, 0xbee5f8763cf933bcU,
  0x3dafc2563e258949U, 0x3eee171a3f1e21deU, 0x3df671813ed6a3dfU, 0xbec4752dbd77a487U,
  0xbee44195be5b60d3U, 0x3ecc6d1a3ed2446dU, 0x3ea4f17f3dd8d62eU, 0xbeef84863e1110c5U,
  0x3e6335ea3e934d68U, 0xbcd5e5a0bf14cb20U, 0x3d9bcfd7bd504bd9U, 0x3ec9c3f13e237ab2U,
  0xbec5e7dfbe165b92U, 0xbe67885a3e0ff259U, 0x3efd008abf7ae9eaU, 0xbecdf0aebd53bfc2U,
  0x3de477ecbf509979U, 0xbed28a5b3e707c0aU, 0x3e119364bc829fe8U, 0xbef786adbe76a831U,
  0x3eb78daa3e7288f4U, 0xbf0f4883bd1151a4U, 0x3e8519cdbe4beee2U, 0x3ee16e4a3df164d5U,
  0xbe3000083ebabc93U, 0x3eb2eb373f5a3f30U, 0xbe118dc53e37fa91U, 0x3df4f091be1276d5U,
  0xbec607333d71d5b0U, 0x3e1f03403d22d101U, 0xbeed3ad33f3c7515U, 0xbeca2575bd73d2b0U,
  0xbe4f028bbea4169dU, 0xbe38c2dc3e526842U, 0x3e8c5053be489094U, 0x3eaa0b0abe9d2fc8U,
  0x3f1dab6cbc865824U, 0x3df5c91dbdf32016U, 0xbeb79a47bf25c2a5U, 0xbf0aab763e0a338cU,
  0xbdaf2225bd5db9a0U, 0xbdd90471bd996ce0U, 0x3e489e37bee1983dU, 0x3e60c980be71d3efU,
  0xbe99bcc93e052aa6U, 0xbee02ec3bef26cceU, 0x3ebad240beaa7481U, 0xbc8c93babeb24af3U,
  0xbe8d16313eaa5ad0U, 0xbe8a972abe099f89U, 0x3db94642be56a745U, 0xbc2f8dc53e1d0d69U,
  0xbec10ffa3a2de2cbU, 0x3f03d6363e22fd77U, 0xbe1aff7b3e38806fU, 0xbe96f92fbe5b2255U,
  0xbe7ac4a0be43bbf8U, 0x3e930487be267fa1U, 0x3d1bd1a8bf27db20U, 0x3e062f353f1b2c45U,
  0xbe9fa995bf0df16cU, 0xbf1201dc3ec51155U, 0xbe1bd7cebde66574U, 0xbe8e2c4bbe6ed3acU,
  0xbe79ac92bd84de12U, 0xbebd50153e0bd7c5U, 0xbe4aed84bf26141eU, 0x3defe98e3e8d313aU,
  0xbe31ab7ebdc1beeaU, 0xbe0c74303f2ef152U, 0x3d12396a3edb17edU, 0xbf0b49d5be25c450U,
  0xbd8e5317bd6b0227U, 0x3e33dd973d7fe8c6U, 0xbeafe4023f52289eU, 0xbd645b62be13853dU,
  0x3f9008d03ea1d22dU, 0x3f2afbc0bb7e7908U, 0xbda45f28bc81b34cU, 0x3ef0ff8e3f2c314fU,
  0xbe72be0e3bd19807U, 0xbd5e9ea53e62756aU, 0xbd28a3093e6da269U, 0xbd699d1d3e6f5a68U,
  0xbdab5df73eb3cafdU, 0x3f20fa9fbddf2e7fU, 0x3b441bea3d9384bdU, 0x3ec286f9bd735612U,
  0x3ea3f5553e96516bU, 0x3e71eebcbe4bf16cU, 0xbeb7a332be81444eU, 0x3ea426b4be03dbf3U,
  0x3f0ad7e9be9cc497U, 0xbe2d2d47bdb1f327U, 0xbec41413be1e61ecU, 0x3ce1e63d3e35ed14U,
  0xbce01041be28dfd8U, 0xbc39aefd3db1774dU, 0xbcc0081e3f30b96cU, 0xbe942d9abeecf785U,
  0x3f2df1b33f0d87cfU, 0xbd445c1bbef69daeU, 0x3dcf469abef052aaU, 0x3f18a152bd161cc9U,
  0xbe8ff0c9beedb098U, 0xbf04781cbef19a5bU, 0xbd5a1a6a3dd892bdU, 0xbe1935273d2bf798U,
  0x3dc3fce6be10b303U, 0xbd6a11c33e7438d0U, 0x3d2e46d43f163b70U, 0x3a244a8c3ed9bce0U,
  0xbf0b8ba1beaa4092U, 0xbea1a664be3fe853U, 0xbebd90a33daaa868U, 0xbf0e40233e71a569U,
  0x3d9c54f8bd386dceU, 0x3e78d17a3f099e8eU, 0xbf3e35bf3ed2b746U, 0x3e0e22d13e868102U,
  0x3f157f00be03dce0U, 0x3de3488c3f31ee93U, 0x3e820a843dc41b89U, 0xbeafe3b9be8abd25U,
  0x3d85ca12be590d39U, 0xbcf7afc0bf08bce0U, 0xbd137f54be889267U, 0x3ea9ec92be28a00fU,
  0x3ebf0bffbda6e902U, 0x3e835aaebec80c95U, 0x3bd98dd03f14c933U, 0xbd3303cdbe588dbaU,
  0x3e91e7493ebaa714U, 0x3c12ee60be1d23cdU, 0xbd5812f7be00c45bU, 0x3f0c42ecbe0ed09dU,
  0xbdd2686d3dd4d27bU, 0x3d6dcb41bf3e31d1U, 0xbeec38f73e95d994U, 0x3e11f8083da17984U,
  0x3ecb904ebf3871baU, 0x3de729f93e8ac436U, 0xbde3e9bc3e1d6708U, 0x3ea7d1d93eea8988U,
  0x3f0a6efbbe87e3abU, 0x3e839b333ea248acU, 0x3e39ae343eb6b8a6U, 0xbec6f4afbf139900U,
  0xbe0a08fc3cce02b4U, 0x3f05f856bf304691U, 0xbe436e46bf09c809U, 0x3e0de770bcfd3087U,
  0xbe8c8cf23f0e4a8fU, 0x3ea86ab73e7102eaU, 0x3e5f1042bc9a8a7fU, 0x3e8dc8563cfe222eU,
  0x3df8656a3b9eba4cU, 0xbe84693dbdf8a029U, 0x3f3f84283e5637eeU, 0xbe80e159bef24e32U,
  0x3ec3249e3e40486dU, 0x3e686050bd8b1761U, 0x3eeb032bbe40960fU, 0xbc9332eebdb952c3U,
  0xbe918d613d9ec1bdU, 0xbdffa9bfbe6d7d80U, 0x3e2ac7dd3c32d798U, 0xbf26e8c33bd1a8e5U,
  0x3f21fb033dcaeeb5U, 0x3d9a14403eb8040fU, 0x3d33044b3e36c0f3U, 0x3f1058253e9bd98aU,
  0x3e92a1b9be22ee31U, 0xbea1be28be36f5afU, 0x3f3831063ed00964U, 0xbe8fccf7be95cbe1U,
  0x3e5dbf163d64ce43U, 0x3e38dee3bc2db2c6U, 0x3dc1870ebe80adf2U, 0x3c75b3013e5e50beU,
  0xbb31c57b3de67755U, 0xbd8698f83e085b99U, 0x3e04fc923e1d0afcU, 0x3df84cabbdd938a2U,
  0x3eed47cf3f112239U, 0xbf85cf313f4b494bU, 0xbefde160bebec334U, 0xbe217115bf554262U,
  0xbd9726babed42a17U, 0xbf89b27abfa316f4U, 0x3e2d476fbf5b175bU, 0x3ad9bc613dd1804dU,
  0xbeba673fbf03eb8fU, 0x3ddb4f84be8e3f50U, 0x3e910adebf13a1b9U, 0x3ecc797c3d0d48d1U,
  0x3e94c60d3d209fbdU, 0x3e939b053e5a8208U, 0x3deffb7d3de81bf9U, 0xbd171cbbbeaf60f1U,
  0xbf03c77a3f0eb78eU, 0x3e410d593e0a9637U, 0xbe1584033eb8e03bU, 0x3f105f71bd1d60bcU,
  0xbee77da53f36dfe0U, 0x3edc575dbd2fc77aU, 0xbf8eaac63e86359eU, 0xbf6788efbf835a7eU,
  0xbf447a04bf1018c5U, 0x3db03895bf2ae1f4U, 0xbe8130833ec31f0bU, 0xbf34bd083e8e1ac8U,
  0x3f2eee0fbe04cab4U, 0xbef080003eb36fc8U, 0x3da720ea3f352126U, 0xbe38e6983e44eeb8U,
  0x3e90cc223e9e7f72U, 0xbeddd14bbe0e76bdU,
};


ai_handle g_meteo_h3_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_meteo_h3_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

