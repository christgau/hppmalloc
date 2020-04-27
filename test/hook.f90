 program hook

   implicit none

   real, dimension(:,:,:), allocatable :: r
   integer, dimension(:,:), allocatable :: i

   !-- use "large" array's that exceed the default allocation threshold
   !-- r => 64 to 128 MB
   !-- i => should be 4+ MB
   allocate (r(256,256,256), i(1024,1111))

   r = 1.0 + 4.0
   i = 7 * 7

   if (.not. all(i == 49)) then
        write (*, *) 'integer failed'
   end if

   if (.not. all(r == 5.0)) then
        write (*, *) 'real failed'
   end if

   deallocate (r, i)

 end program
