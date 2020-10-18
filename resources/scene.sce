# To define a model use the following syntax:
# model <model_name> <filename>|plane|cube
# now model_name can be used to refer to this model.
#
# Similar for a texture:
# texture <texture_name> <filename>
#
# And to define a graphical object that will be part of the scene:
# object <object_name> static|dynamic <previously_defined_model_name> <previously_defined_texture_name> <x-coordinate> <y-coordinate> <z-coordinate>
#
# Use "static" for objects that shouldn't be able to move and "dynamic" for
# objects that can move. 

model spaceship_model spaceship.obj
texture spaceship_diff spaceship_diff.jpg
object spaceship dynamic spaceship_model spaceship_diff 0 0 10

texture normal_map plane_normal_map.jpg
texture pattern pattern.jpg
texture pattern2 pattern2.dds
model plane_model plane

# The syntax for a graphical object that has a normal map defined is the same
# as for object above, except that it's "normal_mapped_object" instead of 
# "object" and that an additional argument at the end is added, a 
# <texture_name> referencing the normal map that should be a previously 
# defined texture.

normal_mapped_object plane1 static plane_model pattern2 -20 -10 0 normal_map
normal_mapped_object plane2 static plane_model pattern2 10 -8 0 normal_map

model cube_from_file cube.obj

# To define multiple object at the same time use array:
# array static|dynamic <model_name> <texture_name> <position_x> <position_y> <position_z> <object_count_x> <object_count_y> <object_count_z> <offset_increment_x> <offset_increment_y> <offset_increment_z>
# Then <object_count_x> * <object_count_y> * <object_count_z> instanced objects
# will be created.

array dynamic cube_from_file pattern 1 0 0 3 3 3 3.0 3.0 3.0
model cube_model cube
array static cube_model pattern 0 0 0 40 40 40 -3.0 3.0 3.0

model platform_model platform.obj

# If a model file (.obj file) references materials defined in an .mtl file, 
# the <texture_name> argument is ignored.
object platform static platform_model not_used 20 -7 4

# To define a light:
# light <position_x> <position_y> <position_z> <focus_point_x> <focus_point_y> <focus_point_z>
light 0 20 5 0 0 0

# To animate an object flying around in a circle:
# fly <previously_defined_dynamic_object>
fly spaceship
fly arrayobject11
fly arrayobject20
