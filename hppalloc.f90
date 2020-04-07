! Fortran module for using hppmalloc C routines.
!
 module hppmalloc

    private

    interface

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
        
    public hpp_alloc, hpp_free

 end module
