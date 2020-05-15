program simple

!-- Test the Fortran/C binding to hppalloc. Under the hood, hppalloc may
!-- use malloc, but this is not the point here. Other tests cover the
!-- allocation strategy.

    use iso_c_binding, only: c_ptr, c_sizeof, c_f_pointer, c_size_t
    use hppalloc

    implicit none

!-- bounds of the arrays
    integer, parameter :: N_LOW = 2
    integer, parameter :: N_HIGH = 5

!-- get kind of size_t
    integer(c_size_t), parameter :: size_t_kind = kind(size_t_kind)

!-- initialization values
    real, parameter :: VALUE_R = 1.23
    integer, parameter :: VALUE_I = 72

    real, dimension(:), allocatable :: f_r
    real, dimension(:), pointer :: hpp_r
    integer, dimension(:), allocatable :: f_i
    integer, dimension(:), pointer :: hpp_i
    integer :: old_mode

    type(c_ptr) :: cptr_hpp_r, cptr_hpp_i

    allocate(f_r(N_LOW:N_HIGH))
    allocate(f_i(N_LOW:N_HIGH))

    old_mode = hpp_set_mode(HPPA_AS_ALL)

    cptr_hpp_i = hpp_alloc(N_HIGH - N_LOW + 1_size_t_kind, storage_size(hpp_i) / 8_size_t_kind)
    cptr_hpp_r = hpp_alloc(N_HIGH - N_LOW + 1_size_t_kind, storage_size(hpp_r) / 8_size_t_kind)

    call c_f_pointer(cptr_hpp_r, hpp_r, [N_HIGH - N_LOW + 1])
    call c_f_pointer(cptr_hpp_i, hpp_i, [N_HIGH - N_LOW + 1])

    hpp_r = VALUE_R
    hpp_i = VALUE_I

    f_r = VALUE_R
    f_i = VALUE_I

    if ( all(hpp_r == f_r) ) then
        write (*, *) "success"
    else
        write (*, *) "failure for real"
    end if

    if ( all(hpp_i == f_i) ) then
        write (*, *) "success"
    else
        write (*, *) "failure for integer"
    end if

#ifdef TEST_VERBOSE
    write (*, *) hpp_r
    write (*, *) hpp_i
#endif

    call hpp_free(cptr_hpp_r)
    call hpp_free(cptr_hpp_i)

    deallocate(f_r)
    deallocate(f_i)

end program
