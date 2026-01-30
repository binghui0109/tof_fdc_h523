/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-01-30T14:04:35+0800
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

#include "network_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_network_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_network_weights_array_u64[150] = {
  0xbe9453683f12c963U, 0x3f31e9c4bdd053e1U, 0x3e5a0455be2aaf99U, 0x3eb71b883dbb883aU,
  0x3f8b6ea23e97dcbaU, 0x3de98113bed100c4U, 0xbd2d7b393e57fefeU, 0xbc60206e3e1b6a60U,
  0x3f5e5ca13eb490d3U, 0x3d41d0f63e06c57cU, 0xbdb833553d3dac54U, 0x3ef54db93ec6bb31U,
  0x3ef12f263e2ba9a6U, 0x3dd87d303ef04410U, 0x3f9a02a9bc88b426U, 0xbeb5cfc8bea2b33cU,
  0xbf807ac8bea62354U, 0xbf8b278bbf26694cU, 0x3e57503b3d98275fU, 0xbe3a5e233f848570U,
  0x3e47f5b4bf20916fU, 0x3e5fb1483f951594U, 0x3dd768b23ef02ab0U, 0x3da831e33d491c7cU,
  0x3f4072263e09c0eaU, 0x3dcd3c1b3e126dc7U, 0x3b893c463e48fcabU, 0xbf1fd697bf404444U,
  0xbe809b123dfda9b8U, 0xbe1be4013f47323eU, 0xbd5e8ec2bf9a6a0eU, 0xbd1047eebfafb950U,
  0xbd523f01bd93ca6eU, 0xbdd64e9dbc0525a6U, 0x3beab5a83e880a40U, 0x3f56d81b3efb91f5U,
  0xbfca14e0bbc6a21bU, 0xbf88f67abbfbf4e0U, 0xc04ddf77bffcd751U, 0xc0501ed63ec8cbceU,
  0xc02d5ae53e811a39U, 0xbf6a7f3a3ee09b80U, 0xc05c6bc3c040ea5fU, 0xc03bc360beb2c962U,
  0xbfe5d71b3f2a1903U, 0xbf05d4ea3f24ef95U, 0xc02875b6bfdc33acU, 0xbfbaaadcbe13d6a8U,
  0xbff635c03f4b45d3U, 0xc0363c653edd9426U, 0xc035a48bc004a0adU, 0xc011a2b4beba0764U,
  0xbfc4df04bdb3e9a8U, 0x3d8270fd3f5d747cU, 0xc04f47c5c02ec3f9U, 0xbfcfe7c2be917b03U,
  0xbfb121883f4d522eU, 0xbf204caa3f3b2caeU, 0xbfba97a0bfa57995U, 0xbf74aefebe8c5cdbU,
  0xbfb913283edcd81dU, 0xbffc31263e8fdbe7U, 0xbfb4321fbffb2c8cU, 0xc03bae5abe9f1f7cU,
  0xc02bcb1c3f16adbfU, 0x3f3859283ec73f95U, 0xc0872cd0c04251a1U, 0xc037a0d63d5572c9U,
  0xc02926823ed3e0afU, 0xbd659b953ed166a4U, 0xbf8db217bffa5ff4U, 0xc08c017c3e2152d8U,
  0xc00e144b3d04c5a6U, 0xbe83ac88bef3f692U, 0xbfe3b8ebc044c522U, 0xc082ea693e8874e4U,
  0x3eecf590be8176dcU, 0x3e15db383d8e4f89U, 0xc03c6baf3e4734d4U, 0xc01057d7bf0243eaU,
  0x3eaf2f35bdcaf52aU, 0xbfd735e53e14c9d6U, 0xc02714643ee129e2U, 0xc02fdae2beb94ad4U,
  0x3ed0224a3da66ecbU, 0xbe4493723dc4da61U, 0xc00031d73ee75ce1U, 0xc02e82ccbf24985bU,
  0x3e983c8cbe5eef04U, 0x3dc3840b3dc32fd3U, 0xc03fc3a03e9d7e30U, 0xc0156f0cbe908296U,
  0x3e2576a8391a55faU, 0x3ea7f2d4bdb4c556U, 0xc07bd7393ed35178U, 0xc0497a6fbd1cfe74U,
  0x3ee097433e422974U, 0x3ebc4da83d334153U, 0xc05a7f4c3e9d36e3U, 0xc039227abda49995U,
  0x3ea52eadbc8283b4U, 0xbbc5403e3dc82e91U, 0xc02b615a3ed13b45U, 0xc01f01babf210d4aU,
  0x3e125f593ea6d6f7U, 0x3e15fb543e3deb2eU, 0xc049f48c3cdca5f2U, 0xc03cdde4bf65d5aaU,
  0x3f22cb1a3eca329fU, 0xbeddb4133e71c1b8U, 0xc062318c3e7886adU, 0xc01ed072bfb86b0cU,
  0x3f21da42bd4c353dU, 0x3f7ca37fbe255b74U, 0x407b85033f00084eU, 0x401974703eac622fU,
  0x3da7762ebec290a5U, 0x3f89ef5fbd6ba458U, 0x4039eefa3e2f060dU, 0x4037bae83f0dbccdU,
  0x3ece06ddbea85506U, 0x4016026ebe43f55aU, 0x40141e1d3f120811U, 0x404cc7643ec7b4caU,
  0x3ed7de1cbdf7466cU, 0x3e97a064be4dbf65U, 0x407a67e13e589a62U, 0x40415701bcd91f16U,
  0xbd8f96abbf011065U, 0x3eea1994bf1685b1U, 0x4083c4ab3e514cf0U, 0x4067894d3f27c050U,
  0x3ebfa3a6bef1f453U, 0x3f758f1ebe9d30b1U, 0x407024043f1c4186U, 0x404c1a1e3eaa704cU,
  0x3e6b3547be3fa72bU, 0x3df09c6dbe884690U, 0x406403263e9aec4eU, 0x4035e3403f2bd79aU,
  0x3dc46decbe53a994U, 0x3e8d0a03be1cb6e3U, 0x40668f123e24f29eU, 0x407dfea83f21c95fU,
  0xbd473255bec364d4U, 0x3f3f695dbe803136U, 0x407371cb3ea815e0U, 0x403b51293f6d9651U,
  0xbf546b4c3fc8e5f3U, 0xbf85e61cU,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

