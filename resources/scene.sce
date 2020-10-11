model spaceship_model spaceship.obj
texture spaceship_diff spaceship_diff.jpg
object spaceship dynamic spaceship_model spaceship_diff 0 0 10


texture normal_map plane_normal_map.jpg
texture pattern pattern.jpg
model plane_model plane
normal_mapped_object plane1 static plane_model pattern -20 -10 0 normal_map
normal_mapped_object plane2 static plane_model pattern 10 -8 0 normal_map

model cube_from_file cube.obj

array dynamic cube_from_file pattern 1 0 0 3 3 3 3.0 3.0 3.0
model cube_model cube
array static cube_model pattern 0 0 0 40 40 40 -3.0 3.0 3.0

model platform_model platform.obj
object platform static platform_model not_used 20 -7 4

light 0 20 5 0 0 0

fly spaceship
fly arrayobject11
fly arrayobject20
