# To define a model use the following syntax:
# model <model_name> <filename>|plane|cube
# now model_name can be used to refer to this model.
#
# If the second texture coordinate (the V in UV) should not be flipped,
# use "model_dont_flip_v" instead of "model".
#
# Similar for a texture:
# texture <texture_name> <filename>
#
# And to define a graphical object that will be part of the scene:
#
# object <object_name> static|dynamic <model_name> <texture_name> <x> <y> <z> <scale>
#
# where <model_name> and <texture_name> should refer to an already defined model
# and texture respectively. If no texture is available, <texture_name> can be
# set to "none" (without the quotes).
# Use "static" for objects that shouldn't be able to move and "dynamic" for
# objects that can move.
# <x> <y> <z> are three floating point numbers that define the position of the object.


model spaceship_model spaceship.obj
texture spaceship_diff spaceship_diff.jpg
object spaceship dynamic spaceship_model spaceship_diff 0 0 10 1

texture normal_map plane_normal_map.jpg
texture pattern pattern.jpg
model plane_model plane.obj
model plane_model_two_sided plane_two_sided.obj
model cube_from_file cube.obj


# The syntax for a graphical object that has a normal map defined is the same
# as for object above, except that it's "normal_mapped_object" instead of 
# "object" and that an additional argument at the end is added, a 
# <texture_name> referencing the normal map that should be a previously 
# defined texture.


# To define multiple object at the same time use array:
# array static|dynamic <name> <texture_name> <x> <y> <z> <c_x> <c_y> <c_z> <i_x> <i_y> <i_z> <scale>
#
# Then <c_x> * <c_y> * <c_z> instanced objects will be created.
# <i_x>, <i_y> and <i_z> are the offset increment distances in each axis.
#
# rotating_array works like array but rotates the objects continuously.
# normal_mapped_array works like array but the objects are normal mapped, the last parameter is
# the previously defined normal map texture.
# normal_mapped_rotating_array is the combination of the two above.

normal_mapped_array dynamic cube_from_file pattern -1 0 0 3 3 3 -3.0 3.0 3.0 1 normal_map
model cube_model cube
array static cube_model pattern 0 0 0 40 40 40 3.0 3.0 3.0 0.4


model platform_model platform.obj

# If a model file (.obj file) references materials defined in an .mtl file, 
# the <texture_name> argument is ignored:

object plane1 static plane_model not_used 20 -10 0 30
object plane2 static plane_model_two_sided not_used -10 -8 0 30
object platform static platform_model not_used -20 -7 4 1

model cube_transparent cube_transparent.obj
object transparent_cube static cube_transparent not_used 0 3 -5 2
object transparent_cube2 static cube_transparent not_used -8 4 -4 1.4

model grating plane_grating.obj
object grating1 dynamic grating not_used -4 3 -3 2
object grating2 dynamic grating not_used -4.2 3.3 -5 2
object grating3 dynamic grating not_used -3.9 3.3 -6 2.5


# To define the view:
# view <position_x> <position_y> <position_z> <focus_point_x> <focus_point_y> <focus_point_z>

view -10 15 -20 0 0 0

# To define a light:
# light <x> <y> <z> <fx> <fy> <fz> <diff_int> <diff_r> <spec_int> <spec_r> <r> <g> <b> <shadow>
# where <x> <y> <z> is the position,
# <f[x|y|z]> is the x, y, and z component of the focus point,
# <diff_int> is the diffuse intensity, <diff_r> is the distance the diffuse light reaches,
# <spec_int> is the specular intensity and <spec_r> is the distance the specular light reaches,
# <r> <g> <b> is the color,
# shadow should be 1 if the light is a shadow casting light, 0 otherwise.

light -10 20 5 0 0 0 4 40 3 50 0.5 0.4 0.2 1
light -20 0 -30 20 7 10 0.5 50 0.8 60 1.0 1.0 1.0 0
light 15 5 -10 -15 0 -10 1 20 1.5 25 1.0 0.0 0.0 0
light 5 7 0 -5 0 0 0.5 18 0.7 20 0.0 0.0 1.0 0

# To set the ambient light:
# ambient <r> <g> <b>
ambient 0.15 0.15 0.15


# To animate an object flying around in a circle:
# fly <previously_defined_dynamic_object> <r_x> <r_y> <r_z> <rot_x> <rot_y> <rot_z> <speed>
# where <r_[x|y|z]> is a point on the radius,
# and <rot_[x|y|z]> defines the axis of rotation.

fly spaceship 12 -4 0 0 1 0 100
fly arrayobject11 17 -4 0 0 0 1 50
fly arrayobject20 12 -3 0 0 1 0 100

rotate arrayobject9
