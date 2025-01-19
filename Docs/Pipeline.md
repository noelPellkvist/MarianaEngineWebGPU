# Pipelines
There are three different built-in bindings that you can setup for automatically retrieving data inside your shader.

## UBO struct
The UBO binding is a binding of some global data (eg. projectionMatrix, viewMatrix and time)

## Modeldata struct
The Modeldata struct would be an array of entries containing the modelMatrix, normalMatrix and material data.

## Bone data
The Bone data is an array containing each bones modelMatrix and normalMatrix in an array

## The PBR built-in pipeline
The built-in render pipeline uses all the three above bindings, with one more for textures.