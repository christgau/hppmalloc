! Fortran module for using hppmalloc C routines.
!
 module hppalloc

    private

    integer, parameter, public :: HPPA_AS_MALLOC = ishft(1, 0)
    integer, parameter, public :: HPPA_AS_ANON   = ishft(1, 1)
    integer, parameter, public :: HPPA_AS_NAMED  = ishft(1, 2)

    integer, parameter, public :: HPPA_AS_PMEM   = HPPA_AS_NAMED

    integer, parameter, public :: HPPA_AS_ALL    = ior(ior(HPPA_AS_MALLOC, HPPA_AS_ANON), HPPA_AS_NAMED)

    integer, parameter, public :: HPPA_AS_NO_MALLOC = ior(HPPA_AS_ANON, HPPA_AS_NAMED)
    integer, parameter, public :: HPPA_AS_NO_ANON   = ior(HPPA_AS_MALLOC, HPPA_AS_NAMED)
    integer, parameter, public :: HPPA_AS_NO_NAMED  = ior(HPPA_AS_MALLOC, HPPA_AS_ANON)

    interface

        subroutine hpp_set_mode(mode) bind(C)
            use iso_c_binding, only: c_int

            integer(c_int), value, intent(in) :: mode
        end subroutine

        type(c_ptr) function hpp_alloc(n, elem_size) bind(C)
            use iso_c_binding, only: c_ptr, c_size_t

            integer(c_size_t), value, intent(in) :: n
            integer(c_size_t), value, intent(in) :: elem_size
        end function

        subroutine hpp_free(ptr) bind(C)
            use iso_c_binding, only: c_ptr

            type(c_ptr), value, intent(in) :: ptr
        end subroutine

    end interface

    public hpp_set_mode, hpp_alloc, hpp_free

 end module
