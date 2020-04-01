 program sizes

     use iso_c_binding

     implicit none

     integer :: i
     integer, pointer :: i_ptr
     integer, allocatable :: i_alloc

     integer, dimension(1:10) :: i_dim
     integer, dimension(:), pointer :: i_dim_ptr
     integer, dimension(:), allocatable :: i_dim_alloc

     write (*, *) "int", sizeof(i)
     write (*, *) "int, pointer", sizeof(i_ptr)
     write (*, *) "int, allocatable", sizeof(i_alloc)

     write (*, *) "int, dim(:)", sizeof(i_dim)
     write (*, *) "int, dim(:), pointer", sizeof(i_dim_ptr)
     write (*, *) "int, dim(:), alloctable", sizeof(i_dim_alloc)

 end program
