# Changelog
All notable changes to this project will be documented in this file. See [conventional commits](https://www.conventionalcommits.org/) for commit guidelines.

- - -
## [4.0.0](https://github.com/rdkcentral/BartonCore/compare/3.1.1..4.0.0) - 2026-04-06
#### Breaking Changes
- **(matter)** Remove Matter.js support from SBMD (#190) - ([7c1c6bd](https://github.com/rdkcentral/BartonCore/commit/7c1c6bd5991a8cf2e58b3b436c32fbefd901254e)) - Thomas Lea
#### Bug Fixes
- **(matter)** Fix attribute reporting for common clusters (#177) - ([63e1c57](https://github.com/rdkcentral/BartonCore/commit/63e1c573585e91db166513b94803e61aa794a53a)) - rchowdcmcsa
- cancel or wait for pending reconfiguration before device removal (#183) - ([c61a6dd](https://github.com/rdkcentral/BartonCore/commit/c61a6ddb12cb26ebf5d30a8104abc91e553fba3c)) - NaeemKK
#### Build system
- **(docker)** Enable multiple clones on the same host (#191) - ([f85224d](https://github.com/rdkcentral/BartonCore/commit/f85224d1131efbe7dcb3b96f22b8733a29e6b7a6)) - Christian Leithner
#### Documentation
- **(style)** added some coding style tips for AI (#182) - ([1e194c6](https://github.com/rdkcentral/BartonCore/commit/1e194c63c19080576fd419d1236dd35c1ebaa8a8)) - Thomas Lea
#### Features
- **(matter)** replace chip-tool with matter.js virtual devices (#195) - ([c76b20d](https://github.com/rdkcentral/BartonCore/commit/c76b20db4169e5b1bfadb548d069d6c94446528a)) - Thomas Lea
- **(matter)** monitor mquickjs internal memory (#187) - ([b8f0133](https://github.com/rdkcentral/BartonCore/commit/b8f0133ffdd9dbda266b0aeb6c0fb2e285402f86)) - Thomas Lea
- **(matter)** add contact-sensor sbmd spec (#186) - ([befa572](https://github.com/rdkcentral/BartonCore/commit/befa5726cc32f55dd23dc1f40c49d709155d306c)) - Kevin Funderburg
- **(matter)** use mquickjs by default (#180) - ([6e94601](https://github.com/rdkcentral/BartonCore/commit/6e946018e7979e182e96985f47b48dde17bef40e)) - Thomas Lea
- **(matter)** resolve SBMD endpoints by device type matching (#174) - ([d5ccdf3](https://github.com/rdkcentral/BartonCore/commit/d5ccdf3b0bafe1b962824c9f48c89dce01ac177b)) - Thomas Lea
#### Miscellaneous Chores
- **(version)** 4.0.0 - ([7036fd3](https://github.com/rdkcentral/BartonCore/commit/7036fd3ff4d02cead355a983f57a0b54d9cb0bf3)) - cog-bot
- push release tag with deploy key (#198) - ([762cf5f](https://github.com/rdkcentral/BartonCore/commit/762cf5f7197cee2cd0b9019b1f8cf8052e6a450b)) - Christian Leithner
#### Refactoring
- **(core)** replace xhCrypto hashing with inline GLib GChecksum (#194) - ([025352a](https://github.com/rdkcentral/BartonCore/commit/025352a882c1b6ab41d004ac1e00233b79a7197a)) - Christian Leithner
- **(docker)** use barton service directly in devcontainer compose model (#196) - ([fec90d9](https://github.com/rdkcentral/BartonCore/commit/fec90d97a684b2b1d38bcbe645d2113ac43149d6)) - Christian Leithner

- - -

## [3.1.1](https://github.com/rdkcentral/BartonCore/compare/3.1.0..3.1.1) - 2026-03-05
#### Bug Fixes
- Consume BartonCommon 0.1.3 (#173) - ([c3a3608](https://github.com/rdkcentral/BartonCore/commit/c3a3608f883d784db8b18dd375bc8bedb67c2f11)) - Christian Leithner
#### Miscellaneous Chores
- enable openspec (#170) - ([31f8d0f](https://github.com/rdkcentral/BartonCore/commit/31f8d0feacb8e5ea7f9f99352967b9804c57e6d8)) - Thomas Lea

- - -

## [3.1.0](https://github.com/rdkcentral/BartonCore/compare/3.0.0..3.1.0) - 2026-03-05
#### Bug Fixes
- **(matter)** Fix forward declarations breakage - ([baffd9d](https://github.com/rdkcentral/BartonCore/commit/baffd9d55e4249c4a4427b9b91025eb07662beae)) - Christian Leithner
#### Build system
- **(docker)** Move sample apps to Dockerfile (#149) - ([2428b41](https://github.com/rdkcentral/BartonCore/commit/2428b41b970fa4df9e0c4f164e7dc0ca40b0a251)) - Christian Leithner
- **(matter)** add schema validation for SBMD drivers (#148) - ([a942267](https://github.com/rdkcentral/BartonCore/commit/a9422674f5056b44bb5b5140a97b15ed0b78d4de)) - Thomas Lea
- Fix SBMD spec installation - ([42bb714](https://github.com/rdkcentral/BartonCore/commit/42bb7147f26947f32a2faf62bb0729d0c7ec2830)) - Christian Leithner
- Add option for SBMD schema validation - ([caa14ab](https://github.com/rdkcentral/BartonCore/commit/caa14ab9261a56816b676611bf99052ef562fc81)) - Christian Leithner
- Fix unguarded test target reference - ([cbc33ae](https://github.com/rdkcentral/BartonCore/commit/cbc33aef726fc233ba136fb65bb8eef5ce1ddd61)) - Christian Leithner
- Clean up some includes and CROSS_OUTPUT - ([4d3effb](https://github.com/rdkcentral/BartonCore/commit/4d3effb7af7d8752d023c2ae9d60a65450a53120)) - Christian Leithner
- Upgrade BartonCommon - ([6952580](https://github.com/rdkcentral/BartonCore/commit/69525805c60ce67833bcd7c22296f5f41493d6b2)) - Christian Leithner
- install relevant sbmd driver files (#169) - ([7e8e893](https://github.com/rdkcentral/BartonCore/commit/7e8e8931fbb1d6c8a46f62ca5885a7aa91c4afe3)) - Thomas Lea
- fix docker/compose.host-network.yaml - ([487cdbf](https://github.com/rdkcentral/BartonCore/commit/487cdbf4ba6ec5683638a592bb45ed2b8fc64ea6)) - Thomas Lea
- facilitate running docker host network (#159) - ([0e7804e](https://github.com/rdkcentral/BartonCore/commit/0e7804ec60e8a692875e537e52a9d7d039d46451)) - Thomas Lea
#### Continuous Integration
- **(docker)** Refactor docker version source (#127) - ([c2a9160](https://github.com/rdkcentral/BartonCore/commit/c2a91609663c9903e22eaace4d2b413337283259)) - Christian Leithner
#### Documentation
- **(matter)** Corrections and cleanup SBMD docs (#166) - ([1809123](https://github.com/rdkcentral/BartonCore/commit/180912355d8e162e7c8ce8535ac4c400db59777f)) - Thomas Lea
#### Features
- **(matter)** Add SBMD driver for Air Quality (#165) - ([5c07e78](https://github.com/rdkcentral/BartonCore/commit/5c07e78010aa835bd4e5be0bdbf75051bedfe93e)) - Thomas Lea
- **(matter)** Add SBMD Water Leak Detector (#164) - ([63d82c5](https://github.com/rdkcentral/BartonCore/commit/63d82c59288c9adc9954e2bf6cb3648e17307f9a)) - Thomas Lea
- **(matter)** Add SBMD driver for Occupancy Sensor (#163) - ([2bce9f6](https://github.com/rdkcentral/BartonCore/commit/2bce9f612041a8f4f5cabaca5f236f1fcd130db5)) - Thomas Lea
- **(matter)** add optional field for SBMD resources (#162) - ([7467c86](https://github.com/rdkcentral/BartonCore/commit/7467c86cefd79c337938fc56d0ffec195a44c700)) - Thomas Lea
- **(matter)** Add event handling (#160) - ([974d068](https://github.com/rdkcentral/BartonCore/commit/974d068f9691ae0d71fa25df456679e942955823)) - Thomas Lea
- **(matter)** enhance SBMD runtime with matter.js (#156) - ([c2049f4](https://github.com/rdkcentral/BartonCore/commit/c2049f491671410fce3380d3b983136bce6896f3)) - Thomas Lea
- **(matter)** add exponential backoff to subsystem initialization retries (#115) - ([6796a5b](https://github.com/rdkcentral/BartonCore/commit/6796a5b9781694849f8aeca69ed0c1ae77139f19)) - Copilot
- **(matter)** add write-commands support for SBMD (#141) - ([687ba01](https://github.com/rdkcentral/BartonCore/commit/687ba01778866bd79c570be56073f0a4208d63e7)) - Thomas Lea
- **(matter)** add SpecBasedMatterDeviceDriver (#129) - ([fe0b287](https://github.com/rdkcentral/BartonCore/commit/fe0b2870b947cf9472ea7612a03f22cedfbcb0c4)) - Thomas Lea
- **(matter)** add SBMD specs dir init param property (#128) - ([949bdab](https://github.com/rdkcentral/BartonCore/commit/949bdabe4e4c440fd6db93e7a1b5fc326f73c6f3)) - Thomas Lea
- **(matter)** add SbmdScript interface and QuickJS implementation (#119) - ([5834f18](https://github.com/rdkcentral/BartonCore/commit/5834f1891e924d08b5d590cf8f1661ede28c98d9)) - Thomas Lea
- **(matter)** SBMD Intro, parser, specs (#118) - ([a6c25aa](https://github.com/rdkcentral/BartonCore/commit/a6c25aa98fba39c7efb437481846d295a6944854)) - Thomas Lea
#### Miscellaneous Chores
- **(docker)** Improve docker image size (#155) - ([0c67797](https://github.com/rdkcentral/BartonCore/commit/0c677974be229f5d8bbbca3de8ca5a7343d40969)) - Christian Leithner
- associate sbmd files as yaml files in vsc (#171) - ([1c4cb67](https://github.com/rdkcentral/BartonCore/commit/1c4cb67dd762b516ae8e34df54c834724b2df750)) - Kevin Funderburg
#### Refactoring
- **(matter)** remove native drivers in favor of SBMD (#146) - ([64c2e0c](https://github.com/rdkcentral/BartonCore/commit/64c2e0c6c53f173dacfa4ff39903ad25f26ce369)) - Thomas Lea
- **(matter)** Add MatterDevice, Update DeviceDataCache (#125) - ([38d80a0](https://github.com/rdkcentral/BartonCore/commit/38d80a05536b97b204ef6592c401738981162e4b)) - Thomas Lea
#### Tests
- **(matter)** Add device_interactor for side-channel (#140) - ([9ecab83](https://github.com/rdkcentral/BartonCore/commit/9ecab83e64ae1c57906eee0039bf4903575daeef)) - Christian Leithner

- - -

## [3.0.0](https://github.com/rdkcentral/BartonCore/compare/2.3.0..3.0.0) - 2026-01-29
#### Breaking Changes
- **(deviceService)** update start discovery ret. (#116) - ([377e40a](https://github.com/rdkcentral/BartonCore/commit/377e40a065b3817b2e6dbe4d91d2f2106b1d2b15)) - Thomas Lea
#### Bug Fixes
- **(matter)** Check for inbound cluster definitions (#112) - ([ec81b42](https://github.com/rdkcentral/BartonCore/commit/ec81b4274ce05cb0b53d7b608992d55585b71158)) - Christian Leithner
- **(matter)** Verify NOC before saving (#111) - ([262d412](https://github.com/rdkcentral/BartonCore/commit/262d41229ac4f054e9b6e6084781cafa07c937a9)) - Christian Leithner
- **(thread)** Dont reset otbr-agent (#113) - ([1b41f9b](https://github.com/rdkcentral/BartonCore/commit/1b41f9bfb618f97d95a9da4a9ed6e8b3f210ed1a)) - Thomas Lea
- Create networkType resource for zigbee devices (#109) - ([4641840](https://github.com/rdkcentral/BartonCore/commit/4641840d889d28d1352d7448747c7f41e19a9f44)) - Munilakshmi97
#### Features
- Matter WiFi signal strength reporting (#101) - ([9c05b22](https://github.com/rdkcentral/BartonCore/commit/9c05b22e49e16737dadf070d43eb560f9f995710)) - rchowdcmcsa
- Matter power source cluster and battery attributes (#92) - ([1e1ce07](https://github.com/rdkcentral/BartonCore/commit/1e1ce07c2203bd289357ec711dba3b6ba1442357)) - rchowdcmcsa
#### Miscellaneous Chores
- **(api)** Fix some GIR warnings (#110) - ([b148e81](https://github.com/rdkcentral/BartonCore/commit/b148e816172f2d9eee6adc5dd6818eb88b9005c4)) - Christian Leithner

- - -

## [2.3.0](https://github.com/rdkcentral/BartonCore/compare/2.2.0..2.3.0) - 2025-12-03
#### Bug Fixes
- **(matter)** return software version string (#106) - ([e9ca522](https://github.com/rdkcentral/BartonCore/commit/e9ca52235e8bf12add50f472dd3ec98d7395f34d)) - Thomas Lea
- Fix RegisterResources() (#108) - ([39e99f9](https://github.com/rdkcentral/BartonCore/commit/39e99f93fc807bdd7a16bf7d6da9fba4efe468b8)) - rchowdcmcsa
#### Build system
- Consume BartonCommon (#104) - ([194b866](https://github.com/rdkcentral/BartonCore/commit/194b866acada6f143e5c6982dacec9b5793b710d)) - Christian Leithner
#### Features
- Matter contact sensor tamper detection (#91) - ([a2f65fa](https://github.com/rdkcentral/BartonCore/commit/a2f65fa368f10d462d9e4c3c68f773d95491e245)) - rchowdcmcsa
- Matter contact sensor driver - ([f86903a](https://github.com/rdkcentral/BartonCore/commit/f86903a51b8416a5b0a86df2e4e587d885215b32)) - rchowdcmcsa
- Matter contact sensor driver - ([2fdc3f5](https://github.com/rdkcentral/BartonCore/commit/2fdc3f5bbf29de6e136b0112d8d3de8fd5aeeac9)) - rchowd807
#### Refactoring
- **(matter)** use wildcard subscription cache (#102) - ([ff308a9](https://github.com/rdkcentral/BartonCore/commit/ff308a90455f0ce322b34299e69f6eb1929c67b5)) - Thomas Lea
#### Reference App
- Fix commissioning reference app (#107) - ([ee6db25](https://github.com/rdkcentral/BartonCore/commit/ee6db25b87b24140c2c72db9010658521f4c2eeb)) - Christian Leithner

- - -

## [2.2.0](https://github.com/rdkcentral/BartonCore/compare/2.1.0..2.2.0) - 2025-11-03
#### Bug Fixes
- **(matter)** Use SetupPayload::generateRandomSetupPin (#98) - ([7e91576](https://github.com/rdkcentral/BartonCore/commit/7e915762dafc1fe3c7e0e4120890a8359d8936fe)) - cleithner-comcast
- release subsystem ref in subsystem test (#79) - ([8cdf845](https://github.com/rdkcentral/BartonCore/commit/8cdf8456b91acf8508cbb3929334360d3668b5ea)) - Kevin Funderburg
- acquire zigbeeSubsystem in test - ([6d9afc1](https://github.com/rdkcentral/BartonCore/commit/6d9afc12a791f486f834311c38e109b03bb97545)) - Kevin Funderburg
#### Build system
- **(matter)** Disable Access Restrictions (#97) - ([d4618f7](https://github.com/rdkcentral/BartonCore/commit/d4618f7d2316533fb285e37618ec1a5277960184)) - cleithner-comcast
- **(matter)** Backport thread stack change (#95) - ([71ff0f9](https://github.com/rdkcentral/BartonCore/commit/71ff0f9ee9682a1d9a884387dd964ad4e87b7ca8)) - cleithner-comcast
- standardize on docker compose commands (#100) - ([46e1d4d](https://github.com/rdkcentral/BartonCore/commit/46e1d4de34ccd457b2483004d80272ededb8c978)) - Kevin Funderburg
- add option for OTA Provider support (#99) - ([1287c4a](https://github.com/rdkcentral/BartonCore/commit/1287c4a3c0349edb4a8ae8ffca3614795cee21f7)) - Thomas Lea
- remove BCORE_PROVIDE_LIBS option (#87) - ([ca5766f](https://github.com/rdkcentral/BartonCore/commit/ca5766f14c6019aacf8ea5e285ef73dd082dd7c3)) - Kevin Funderburg
- promote `zigbeeEventTracker.h` to public (#85) - ([0d9bbe0](https://github.com/rdkcentral/BartonCore/commit/0d9bbe02ee636622aeb382ebf8a8f71b29fa0a2c)) - Kevin Funderburg
- remove private API cmake (#84) - ([b571067](https://github.com/rdkcentral/BartonCore/commit/b5710672280628574f0a7540394c1a3771275f4b)) - Kevin Funderburg
#### Features
- **(matter)** Upgrade to using Matter 1.4.2.0 (#86) - ([6237e72](https://github.com/rdkcentral/BartonCore/commit/6237e72253e9210bde36e49b6bc66a2f9053bdf4)) - cleithner-comcast
- **(zigbee)** implement watchdog delegate interface (#67) - ([a292d5d](https://github.com/rdkcentral/BartonCore/commit/a292d5d579623fdededc0c61e3f1c2e81058842c)) - Kevin Funderburg
- add allowlist aggressive update property (#89) - ([e4cf2cc](https://github.com/rdkcentral/BartonCore/commit/e4cf2cc35534c7b7e842d0c8aa656f0a9738a89b)) - Kevin Funderburg
- migrate zigbee telemetry to property provider (#88) - ([3b564bc](https://github.com/rdkcentral/BartonCore/commit/3b564bc50f9eaed9ec2406ab081728c4b86f6960)) - Kevin Funderburg
#### Refactoring
- Clean up Matter subscriptions (#93) - ([d2a35fe](https://github.com/rdkcentral/BartonCore/commit/d2a35fe857f053ffa942dbc127eb794212e6118c)) - Thomas Lea
- remove MAP_FOREACH macro (#83) - ([a729c66](https://github.com/rdkcentral/BartonCore/commit/a729c66fded8790431f1cb62e6786ccc8724001d)) - Kevin Funderburg
- rip out software watchdog - ([a0f8047](https://github.com/rdkcentral/BartonCore/commit/a0f804784cb2ae33e9d281abc0949c680820f471)) - Thomas Lea

- - -

## [2.1.0](https://github.com/rdkcentral/BartonCore/compare/2.0.0..2.1.0) - 2025-10-01
#### Bug Fixes
- Redact ZHAL network blob from debug prints (#74) - ([06181a7](https://github.com/rdkcentral/BartonCore/commit/06181a7f9c8bb391ff29ebb646907f086eda3796)) - Thomas Lea
- Remove multicast receiving from ZHAL (#73) - ([4f296a8](https://github.com/rdkcentral/BartonCore/commit/4f296a8d15b3882962858137c336d88f2dfa6eb7)) - Thomas Lea
#### Build system
- move zhal from libs to src (#78) - ([1980682](https://github.com/rdkcentral/BartonCore/commit/19806828629c747ef9de7df406592846be2c0ea5)) - Thomas Lea
- update test environment gir to 2.0 - ([3be7e29](https://github.com/rdkcentral/BartonCore/commit/3be7e2936f48eff648e8bd3f9b59ee350a85b67f)) - Thomas Lea
#### Continuous Integration
- Update check-pr-title to use cocogitto (#76) - ([6fc8a4b](https://github.com/rdkcentral/BartonCore/commit/6fc8a4be73d7c36845ba86ddb36d9ef87a408e7b)) - cleithner-comcast
#### Features
- redesign subsystemManager with clear lifecycle (#50) - ([7aa6028](https://github.com/rdkcentral/BartonCore/commit/7aa60288411540c2010f55e95ddbe183a13b9110)) - Kevin Funderburg
- implement subsystem reference counting (#49) - ([d09123e](https://github.com/rdkcentral/BartonCore/commit/d09123e1af6e8e1ee5501cc96aaee72ae8ffed7b)) - Kevin Funderburg
- add VS Code tasks for running tests (#77) - ([d176def](https://github.com/rdkcentral/BartonCore/commit/d176def9931e952dd17fad85c6ff2b77c55a419d)) - Kevin Funderburg
#### Miscellaneous Chores
- promote `app` to first class conventional commit type (#81) - ([badcd29](https://github.com/rdkcentral/BartonCore/commit/badcd2913cd6be656c8552e37a1feec008268895)) - cleithner-comcast
#### Reference App
- Add configuring storage dir (#82) - ([26554fa](https://github.com/rdkcentral/BartonCore/commit/26554fabb1a3d7db3c258dfee20507587d56f118)) - cleithner-comcast
#### Tests
- Use dynamic GIR version in integration tests (#75) - ([ef06ec3](https://github.com/rdkcentral/BartonCore/commit/ef06ec3d09c0a0db540b4b17c6093314f0fc4e8f)) - cleithner-comcast

- - -

## [2.0.0](https://github.com/rdkcentral/BartonCore/compare/1.1.0..2.0.0) - 2025-09-02
#### Breaking Changes
- Remove CertifierDACProvider.cpp - ([2180622](https://github.com/rdkcentral/BartonCore/commit/21806224a2a08599e9b5b3921e5b9ef1b037dd25)) - Christian Leithner
#### Bug Fixes
- **(thread)** fix retrieving device role (#53) - ([90ad898](https://github.com/rdkcentral/BartonCore/commit/90ad898923ead8d52a97f55365203a922375b909)) - Thomas Lea
- remove unneeded test CD key (#64) - ([a5a97a6](https://github.com/rdkcentral/BartonCore/commit/a5a97a694a669b9823bdfc9e0b059c0cff5a718b)) - Thomas Lea
- shutdown race in zhal (#61) - ([8017490](https://github.com/rdkcentral/BartonCore/commit/801749050f6aec246efbeb42b29167e7adcd5494)) - Thomas Lea
- cleaned up an unused variable (#57) - ([1356688](https://github.com/rdkcentral/BartonCore/commit/135668842e0e70d490669291d7d56b3a08551214)) - Thomas Lea
#### Build system
- Remove libcertifier from Dockerfile - ([a24a504](https://github.com/rdkcentral/BartonCore/commit/a24a504249268fa0511a056bd39d7ac933a92dee)) - Christian Leithner
- Disable BCORE_GEN_GIR (#63) - ([ba2b1eb](https://github.com/rdkcentral/BartonCore/commit/ba2b1eb3a6aeef52989fac2143cb24b220a2e570)) - cleithner-comcast
- Set SO Version (#62) - ([075bbf2](https://github.com/rdkcentral/BartonCore/commit/075bbf2e5c21486d1ac5805588bf0de639753f15)) - cleithner-comcast
- Support BCORE_LINK_LIBRARIES (#58) - ([46527bb](https://github.com/rdkcentral/BartonCore/commit/46527bb77216b9e4fa3cbf39b318b6d70758c0a9)) - cleithner-comcast
- turn fff dependency check back on (#56) - ([e5318f2](https://github.com/rdkcentral/BartonCore/commit/e5318f27264c5d946c896817be109303258fae62)) - Kevin Funderburg
#### Continuous Integration
- Add cocogitto support to actions (#72) - ([bdb58a5](https://github.com/rdkcentral/BartonCore/commit/bdb58a588a370f202bcffb24664323576133f57e)) - cleithner-comcast
- Add CLA check (#37) - ([ec46233](https://github.com/rdkcentral/BartonCore/commit/ec46233aa99e9e77d73dfe69980e5b78eac12b32)) - rdkcmf
#### Documentation
- **(architecture)** Update architecture docs (#68) - ([eec0369](https://github.com/rdkcentral/BartonCore/commit/eec0369ab10ff0ea939bbb9687686266c4c77e27)) - Thomas Lea
#### Miscellaneous Chores
- add release action (#70) - ([7078094](https://github.com/rdkcentral/BartonCore/commit/707809444e4ce7bbee6c34de965eaf81fbe545cc)) - cleithner-comcast
- Update mentions of "device service" (#69) - ([11a8f11](https://github.com/rdkcentral/BartonCore/commit/11a8f1148fd4c5af301dd13005e0c4a46d71f017)) - cleithner-comcast
- remove deviceServiceGatherer from barton (#51) - ([7229d61](https://github.com/rdkcentral/BartonCore/commit/7229d612efce736dc1e492a7367af9655ba556dc)) - Munilakshmi97
#### Refactoring
- deviceServiceClass (#59) - ([d8832ec](https://github.com/rdkcentral/BartonCore/commit/d8832ec2037e4d311c1f7a5b62bdbdedc700eeda)) - Kevin Funderburg
#### Revert
- "chore: add release action" (#71) - ([256f5ed](https://github.com/rdkcentral/BartonCore/commit/256f5ed978b3b869f10431f67f1b8394f4714900)) - cleithner-comcast

- - -

## [1.1.0](https://github.com/rdkcentral/BartonCore/compare/1.0.0..1.1.0) - 2025-07-30
#### Bug Fixes
- **(driver)** correct values for illuminance and humidity (#41) - ([1ef0a94](https://github.com/rdkcentral/BartonCore/commit/1ef0a94d968d0a2a4278320dd1ae12985d64797d)) - Micah Koch
- **(matter)** support ARL feature as optional (#46) - ([89d28fc](https://github.com/rdkcentral/BartonCore/commit/89d28fc610103b9bfd8339f1bd769e4bd3b5ae26)) - Thomas Lea
#### Build system
- **(yocto)** add support for barton-matter recipe (#43) - ([9a71050](https://github.com/rdkcentral/BartonCore/commit/9a710502afd0c8754bfeac1dc01516a72e2f0b1a)) - Kevin Funderburg
- Add BCoreMatterHelper (#48) - ([908d8ab](https://github.com/rdkcentral/BartonCore/commit/908d8ab4625a4377918dc44e23e9402452ffa0bc)) - cleithner-comcast
- Fix bad default option handling (#45) - ([f3c7739](https://github.com/rdkcentral/BartonCore/commit/f3c77397c909957bfb8d92eb2f3b23abafc3e464)) - cleithner-comcast
- add fff to build environment (#40) - ([e75fa9f](https://github.com/rdkcentral/BartonCore/commit/e75fa9f5bde48a261076d1204a11f1e811ff2a68)) - Kevin Funderburg
#### Documentation
- replace old `brtn` usages (#44) - ([99fb19d](https://github.com/rdkcentral/BartonCore/commit/99fb19d6cec8ac09e3560386e5fc04968b2e189d)) - Kevin Funderburg
#### Features
- **(matter)** Generate passcode if required (#52) - ([606a93a](https://github.com/rdkcentral/BartonCore/commit/606a93ac6d0d2310d8fe0a922f06c6cefb7c0668)) - Thomas Lea
- **(matter)** BartonDeviceInstanceInfoProvider (#42) - ([de91c60](https://github.com/rdkcentral/BartonCore/commit/de91c6048036f264b57711011c55708c4c78c619)) - Thomas Lea
#### Miscellaneous Chores
- consider older branch in post-checkout hook (#47) - ([66ae888](https://github.com/rdkcentral/BartonCore/commit/66ae8888d0e4506f77bd67da7e85f3f29e62ca73)) - Kevin Funderburg
- remove remoteCellModemCluster from Barton (#34) - ([8c2d116](https://github.com/rdkcentral/BartonCore/commit/8c2d1163cc29a682416c8e98f20989e97e174eb5)) - Kevin Funderburg

- - -

## [1.0.0](https://github.com/rdkcentral/BartonCore/compare/0.9.3..1.0.0) - 2025-07-02
#### Breaking Changes
- Refactor out public "device service" naming (#30) - ([79aad6b](https://github.com/rdkcentral/BartonCore/commit/79aad6b42e5123be7486828bf0f850ac11dafdf3)) - cleithner-comcast
#### Bug Fixes
- **(matter)** Fix possible null dereference (#29) - ([39d1e31](https://github.com/rdkcentral/BartonCore/commit/39d1e3171c50dc7d44c4bdb3d2fa82386e416135)) - cleithner-comcast
- **(props)** case insensitive bool props (#33) - ([fd80e8f](https://github.com/rdkcentral/BartonCore/commit/fd80e8fb6117f60c512a5f495f8946562b03b4a3)) - Thomas Lea
- prevent segfaults in subsystemManager (#39) - ([3968e16](https://github.com/rdkcentral/BartonCore/commit/3968e1610304326a31e2f94078e6891ccad6efa6)) - Kevin Funderburg
- Don't reference BartonProjectConfig.h (#22) - ([ab765e3](https://github.com/rdkcentral/BartonCore/commit/ab765e32648d9b593b4423c322404432dc1e9553)) - cleithner-comcast
#### Build system
- Reorganize matter unit tests (#19) - ([25e226e](https://github.com/rdkcentral/BartonCore/commit/25e226e60311f98802d10bf45a0e03b0351f3c57)) - cleithner-comcast
- Reorganize most test targets (#18) - ([e5a3d91](https://github.com/rdkcentral/BartonCore/commit/e5a3d9108b4a3b14c87b30c03dd150a3a239508c)) - cleithner-comcast
- improve Docker tag management and container warnings (#20) - ([fece80a](https://github.com/rdkcentral/BartonCore/commit/fece80aa0794db66340071e36e180aa8deff14ab)) - kfundecmcsa
- Add missing provider implementations (#17) - ([64439c5](https://github.com/rdkcentral/BartonCore/commit/64439c5be258c1934fc4e3ae23366c53f46919e2)) - cleithner-comcast
- Fix Adding CMake Options to build.sh (#11) - ([ac69927](https://github.com/rdkcentral/BartonCore/commit/ac69927e5a146f0fe2176888490167b62fd68546)) - cleithner-comcast
#### Features
- **(core)** Add ZigbeeRemoteCliResponseEvent (#23) - ([7386a3f](https://github.com/rdkcentral/BartonCore/commit/7386a3f048d9461206286c216e3ee7ea1e7f1d00)) - ganeshInduri434
- **(matter)** Client provided commissioning params (#32) - ([b6dc7c2](https://github.com/rdkcentral/BartonCore/commit/b6dc7c25a966213a0cb76f3a8f3d9c3bb38925fd)) - Thomas Lea
- make Matter BLE Controller device name configurable (#36) - ([f3b631d](https://github.com/rdkcentral/BartonCore/commit/f3b631d294df45480d5b5fa3e636113eaa56306a)) - Kevin Funderburg
#### Miscellaneous Chores
- **(hooks)** Update clang-format-hooks (#24) - ([6d87a12](https://github.com/rdkcentral/BartonCore/commit/6d87a1286a7f4cd1c26d662f9dacc54af148300a)) - cleithner-comcast
#### Refactoring
- **(core)** Use GLib containers in subsystemManager (#8) - ([68c30f1](https://github.com/rdkcentral/BartonCore/commit/68c30f1b733ff40594480f8edbd52936c434ffac)) - ganeshInduri434
- **(core)** Use GLib containers in deviceCommunicationWatchdog (#9) - ([4afb7be](https://github.com/rdkcentral/BartonCore/commit/4afb7be7f6179d873a59d0e697cbf7de797e105a)) - Munilakshmi97
#### Tests
- Fix random assertion failure (#31) - ([d95f2a6](https://github.com/rdkcentral/BartonCore/commit/d95f2a64f4d2485bf96e08b33a48dfe3e270fd4f)) - cleithner-comcast
- Unit tests for subsystemManager (#15) - ([db32b7f](https://github.com/rdkcentral/BartonCore/commit/db32b7fe4f6ece617656ff627d50fb52c6b40ab7)) - ganeshInduri434
- Unit tests for deviceCommunicationWatchdog (#16) - ([0d24ad6](https://github.com/rdkcentral/BartonCore/commit/0d24ad69930d1d9a899e75c0fc85298bc658d576)) - Munilakshmi97

- - -

## [0.9.3](https://github.com/rdkcentral/BartonCore/compare/93f22cead76803307396ed06b5cf6eb9f1629d64..0.9.3) - 2025-05-20
#### Bug Fixes
- **(build)** Ensure pygobject stub generation (#1) - ([be49ffd](https://github.com/rdkcentral/BartonCore/commit/be49ffdd48be02e15f3196620ce297e09ff4794f)) - cleithner-comcast
- **(ci)** No longer depend on secret for CI checkout (#3) - ([b1cbe93](https://github.com/rdkcentral/BartonCore/commit/b1cbe93268975433c35e7756dba4bd6538775608)) - cleithner-comcast
- Fix LSAN suppressions to detect leaks again (#2) - ([bab2272](https://github.com/rdkcentral/BartonCore/commit/bab227258cf2a90296cc77bb8fde67fdeee31e0c)) - cleithner-comcast
#### Build system
- Downgrade required cmake to 3.16.5 (#12) - ([baef496](https://github.com/rdkcentral/BartonCore/commit/baef4966c3d844da2f5073543f5c7cf8e394e69c)) - cleithner-comcast
- ensure image is built by default - ([f18715e](https://github.com/rdkcentral/BartonCore/commit/f18715ea2c2ba3fd4b51acda2e4af3e12e5e6aa0)) - Kevin Funderburg
- add support for rdk yocto builds - ([fd1fb6e](https://github.com/rdkcentral/BartonCore/commit/fd1fb6e8256a387aba9d09d796c3651a22f50084)) - Kevin Funderburg
#### Documentation
- **(architecture)** Add docs/architecture - ([6d8d33a](https://github.com/rdkcentral/BartonCore/commit/6d8d33a98edcdd1f5d3feb3569e33c83198cd4e3)) - Thomas Lea
- **(architecture)** Add docs/architecture - ([e458922](https://github.com/rdkcentral/BartonCore/commit/e45892289ef84a681dd3a7ad0b03a72c4d161d58)) - Thomas Lea
#### Features
- **(ref)** Set the wifi creds from ref app's cli (#7) - ([28f990f](https://github.com/rdkcentral/BartonCore/commit/28f990f18ad459149dbb83acc6fec02f2bb18f64)) - Munilakshmi97
#### Miscellaneous Chores
- Add .gitmessage on hooks/install.sh (#6) - ([fd3ac38](https://github.com/rdkcentral/BartonCore/commit/fd3ac3826176be4ebb37d4695fe8c8a6bd8f6c0d)) - cleithner-comcast



Changelog generated by [cocogitto](https://github.com/cocogitto/cocogitto).
