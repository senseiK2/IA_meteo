/**
  ******************************************************************************
  * @file    meteo_h0_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-11-14T11:38:00+0100
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

#include "meteo_h0_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_meteo_h0_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_meteo_h0_weights_array_u64[242] = {
  0xbf3f682b3f50cff9U, 0xbde70f293eada4ecU, 0xbd14cb8e3da70d86U, 0x3e741d213e73c9eaU,
  0xbe7bfe8d3f1c5266U, 0xbe6b9d27be4fc46eU, 0x3ee41c1b3e8048ccU, 0x3f37a0223e35f117U,
  0x3e04a3d03ea9763fU, 0xbdc29d40bdeb9171U, 0xbf11fc52be287680U, 0xbe4819043d14b707U,
  0xbf0385803efb81d5U, 0xbe35e536bdd17aacU, 0x3c01823bbd11fb3cU, 0x3e75cd88bd54c6f0U,
  0xbf135cd5be0c3c08U, 0xbeca6a503d5eec52U, 0x3e290cd63ecb73b9U, 0x3f368fe3be8df4bbU,
  0x3f5cca91bd1abe5aU, 0xbf310814be5f84a9U, 0xbe624248bc24c084U, 0xbda25d273e133986U,
  0x3e6f8dca3e711212U, 0x3edcee5d3edb0db3U, 0x3d8f75ea3f1841f9U, 0xbe8ac4b73da2e550U,
  0xbdbc3fafbea322ceU, 0x3c9c497cbd9d82a0U, 0x3f38251fbedae3dbU, 0xbf0ead10bd8120edU,
  0x3e8dbad33eb86b16U, 0x3cb8b6d8bef9edd8U, 0x3d4a74f3bc909c50U, 0x3ec115103e36067dU,
  0xbf58e1b8bceca663U, 0xbdc85e9f3ed3a652U, 0x3e9941c23d9f578aU, 0xbea6539f3e726a76U,
  0x3e2f0b5abf59818dU, 0xbedd30513e1b0475U, 0x3dbb4ad03cd99d22U, 0xbed29dabbe78d039U,
  0x3eaaf2513e42ad7eU, 0xbef1d05dbc477a5bU, 0x3e190ec13d048a76U, 0x3ee8c8953c9a93f4U,
  0xbc3e1e843e039a11U, 0x3eb8aed73f49fe6eU, 0x3e1034113c205b63U, 0x3d8e15c5bed749c8U,
  0xbebb7ba03d49f277U, 0x3e531452bc81c254U, 0xbed82ce83ef9489fU, 0xbea390b93ce48744U,
  0xbdecbe1a3dac447aU, 0xbc96889a3d3ee654U, 0x3f6979a3be20f8baU, 0xbc7afc54bc9922aeU,
  0x3f0331bbbe8adef5U, 0xbc24c42dbde02c1aU, 0xbf44a890beb12603U, 0xbf2a0d203db0f1bfU,
  0x3d654636be5f11e9U, 0x3f19d9e23e6d8d9eU, 0xbeb35939bef1e27aU, 0xbe8e6992bf012944U,
  0x3e0eab963e1612e3U, 0xbf1c9fb3bf19f604U, 0x3ea0d927bd15453dU, 0xbec81a74bde08772U,
  0xbdc205933f208a58U, 0xbe0b44d8be1b8c1aU, 0x3eb15aa6be7cbd3cU, 0xbec6ba673e0c6e94U,
  0xbe697359be1961c3U, 0x3ed01a233d94f2daU, 0xba3f63d73cbf0afbU, 0x3c0b361ebec40168U,
  0xbf08aa41be2a8664U, 0x3e9715cebe9dd3c0U, 0x3df21a89beae7c3cU, 0xbe8f2af23f0873fbU,
  0x3eb273d4bf55f421U, 0xbf43fe8e3eafe50dU, 0xbdf8251e3e775f71U, 0xbf0216cfbdef4c36U,
  0xbe9439f7bb271d8eU, 0x3d0c91a83df15225U, 0xbe94c963be628c86U, 0xbef07ae33ed8d5f4U,
  0x3d03f1673e1a7df6U, 0x3e9845fc3f4865e7U, 0x3d4db0483e98d003U, 0xbe524ceabef27db8U,
  0x3f09b8bbbe473512U, 0xbce6972ebe097cffU, 0xbd735f313f4c10dcU, 0x3fa6f6c83d4ec5c2U,
  0x3d8620aa3ec8f501U, 0x3f6426a93c561e88U, 0xbe610d95bd45d3f6U, 0x3cef31203f99b5a7U,
  0xbcf282f2bf1de82dU, 0xbe680f783e2b2556U, 0x3e87a9af3dc1b986U, 0x3dfaacddbed2eee0U,
  0xbe27b1293e7b36cfU, 0x3ef24c7abeb0e733U, 0xbe5d81b23dbf4f7dU, 0x3f1f856b3c0f4a53U,
  0x3d21f352bcc686c3U, 0xbe84539bbe80ef07U, 0x3de4fc5d3ed192e6U, 0x3ed6bbfabd6d2205U,
  0x3eb550b2be82f356U, 0xbd018fee3f12bac1U, 0xbeb13d1d3edc8a85U, 0xbe7d335a3ee62a64U,
  0xbf6dfe92be9ca711U, 0xbdfe07113e8929dbU, 0x3da5aace3d840c92U, 0x3dfc772dbf14f73fU,
  0xbe366ce23f108dcbU, 0xbebcb03ebec3444eU, 0x3cf5b6a5bf07e1e8U, 0x3f14c84c3ec4f3d2U,
  0xbeac5ed23c46f2f6U, 0xbecc0172beac024fU, 0xbefd70b9bebedbf4U, 0xbe4e6fddbe0cdc9fU,
  0xbe8c9ba93e3e82e5U, 0xbe066988be14b20bU, 0xbeddd5673e51c5daU, 0xbd87faf13d52e783U,
  0xbe834cd0bf5c585aU, 0xbf35df70be52f135U, 0x3e1b790cb8ff84dfU, 0xbe3b10ffbeaaabccU,
  0xbf3698433dcfe4eeU, 0x3eddff9d3f1b4904U, 0xbf8a03f13ed22180U, 0xbdc647903dc6491aU,
  0x3eddf05c3daba427U, 0x3f0d88003f50c439U, 0xbd5f3b9fbe3fcb2aU, 0x3d8ad83fbe578321U,
  0x3e2b3ca3be9d924eU, 0xbe22422cbeaed183U, 0x3e14276cbd4119d6U, 0x3ec394afbdc5f8b1U,
  0x3e9aed52bdbcd542U, 0x3e09b1a5be73f518U, 0xbe5e69f03db52b93U, 0x3e94676cbd50179cU,
  0xbd0d71003e178481U, 0xbea14c9dbeb30ed1U, 0x3b62f27fbdb8c78fU, 0x3f58c0503e264031U,
  0xbeb422fdbd355f4bU, 0x3d3dc19fbd8e6b6cU, 0xbefa63aa3f343699U, 0x3f0154e9bd1e6d23U,
  0xbd537fe4bf28d376U, 0x3dce5ded3eab073fU, 0xbd5bdf4b3e4a8d49U, 0x3cb411743ee2af8dU,
  0x3f4ae33dbe5ffa9dU, 0xbdacf2c43e6a2353U, 0x3e19b80a3eafdedcU, 0x3f1e3790bec2930bU,
  0xbe65e1f0bd2d90baU, 0x3f153ce7bedffc3cU, 0xbd9cfe89be6af4c1U, 0x3dc607423ea774e1U,
  0x3c9e49b23efc600bU, 0x3f17f8a53ebbd381U, 0xbe18b3c9bdd17985U, 0x3dca230abdb239b9U,
  0x3efe6f903d270681U, 0xbdd1a73ebdf3bfbaU, 0x3eb509ba3ca384d2U, 0xbe6b2bccbdce0becU,
  0x3ef6c753bde7ac6bU, 0x3e0dcbb5beca18dcU, 0x3e49c2d13e0246d3U, 0xbe86a7a3bdffdbf7U,
  0xbe1408253e878d11U, 0x3e9eada9bf07af70U, 0xbd86c56b3d02815aU, 0xbf3fd80abd0080e9U,
  0x3d8786e9be8769b1U, 0x3d3222d03e13ae32U, 0xbf2a97183d582243U, 0x3eaf04bf3edfc7faU,
  0x3f16bcecbe896280U, 0xbf188b65be72f0f9U, 0x3df313073e9b95c2U, 0xbf060fe73cd313f8U,
  0x3e8dae42bd33feaaU, 0x3d241568be2078caU, 0xbc54b77dbd9d7e1aU, 0xbe5a82c03e44a62fU,
  0x3e2a36e6bd1f5af7U, 0x3be05ec93dfd2646U, 0x3e8fc6523e4c8067U, 0x3e88ab883c9fa32eU,
  0x3f06df5b3f37f1bfU, 0xbf9b58393f39b4a7U, 0xbf11e266bf0c8873U, 0x3d767a3fbf2e924eU,
  0x3eb4663fbeaf555bU, 0xbf913febbf45e476U, 0x3e0f49a8bf923eceU, 0x3df9fdaabfaf42c9U,
  0xbe46c042bf6498fdU, 0x3e15d724bf012ba8U, 0x3dfe693bbf11a1e2U, 0x3ed3dd86bc689172U,
  0x3e93d2743e4fbbf7U, 0x3e9832a03e8196deU, 0x3db3eb6d3e8a42d8U, 0x3d6a0e2bbf06677aU,
  0xbecf7dd63f07a8c6U, 0x3e211af03f00b371U, 0x3e6bdf563ec63155U, 0x3d22297dbee142ddU,
  0xbf622bd23f43ed85U, 0x3ecad8e2bf225b78U, 0xbf74b6d43eab78b7U, 0xbf1dde0dbc8f6084U,
  0xbf81473fbec788f3U, 0x3dfd6de0bee3b53dU, 0xbe9f64953edcc363U, 0xbd6cb49a3ef4a622U,
  0x3edbd16bbf0cea57U, 0xbed0af993f011e0dU, 0x3d9acbfd3f0b9917U, 0xbeefe9263eaac83dU,
  0x3eaff05b3e53eaeeU, 0xbec26fc9be23d41aU,
};


ai_handle g_meteo_h0_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_meteo_h0_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

