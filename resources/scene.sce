model spaceship_model spaceship.obj
texture spaceship_diff spaceship_diff.jpg
object spaceship dynamic spaceship_model spaceship_diff 0 0 10

texture pattern pattern.jpg
model plane_model plane
object the_plane static plane_model pattern 0 -10 0

model cube_from_file cube.obj

array dynamic cube_from_file pattern 1 0 0 3 3 3 3.0 3.0 3.0
model cube_model cube
array static cube_model pattern 0 0 0 40 40 40 -3.0 3.0 3.0

fly spaceship
fly arrayobject11
fly arrayobject20