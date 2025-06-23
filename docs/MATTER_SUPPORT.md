# Adding Matter Support to Barton

Barton is, itself, a library. In order to manage a Matter stack, it must also link against a library version of the Matter SDK. Matter predominantly expects to build applications. Barton provides a build target for the sdk, located in `third_party/matter/barton-library`, that builds a static archive capable of providing Matter SDK functionality to a Barton client application in a development environment. This matter library is built by default for a developer opening the project in VSCode for the first time. Subsequent rebuilds are not necessary.

It is very likely clients of Barton may want to pick and choose their matter application configuration, or provide custom code. They would then need to generate their own custom Matter SDK target library and point Barton to it (see: `BCORE_MATTER_LIB`). The Barton matter library build target thus also acts as a reference for clients to create their own Matter libraries.

At this time, Barton fixes on Matter's v1.4.0.0 release.
