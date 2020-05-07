 program shadow
!
!- Just call hpp_set_mode function and leave allocations as they are.
!- Since hpp_set_mode is used, we need to link against libhppalloc.so,
!- but as allocations are left as they are they do not go to the HPPM
!- allocator. However with LD_PRELOAD this should be the case. This is
!- what the test is about.

   use hppalloc

   implicit none

   real, dimension(:,:,:), allocatable :: r
   integer, dimension(:,:), allocatable :: i

   !-- use "large" array's that exceed the default allocation threshold
   !-- r => 64 to 128 MB
   !-- i => should be 4+ MB

!- disable malloc for subsequent allocations. Without LD_PRELOAD
!- this has no effect besides changing the internal state of the lib.
   call hpp_set_mode(HPPA_AS_NO_MALLOC)

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
