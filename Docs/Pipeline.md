# Pipelines
The first three bindings are constants in the following order.

## UBO struct
The UBO binding contains projection and view matrices, the current time and lights info about the current scene.

## Modeldata struct
The Modeldata is always on binding 1, containing the models modelMatrix, normalMatrix and material properties.

## Bone data
The Bone data is always bound on binding 2, containing each bones modelMatrix and normalMatrix in an array

## The PBR built-in pipeline
The built in PBR pipeline defines the rest of the bindings according to textures