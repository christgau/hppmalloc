program simple

    use iso_c_binding, only: c_ptr, c_size_t
    use hppalloc

    implicit none

!-- bounds of the arrays
    integer, parameter :: N_4k = 6 * 256 !-- 6kB, integers have 4 Bytes by default 
    integer, parameter :: N_2M = 6 * 1024 * 256 !- 6MB 
    integer, parameter :: N_1G = 1024 * 1024 * 256 ! 1GB

!-- get kind of size_t
    integer(c_size_t), parameter :: size_t_kind = kind(size_t_kind)

    type(c_ptr) :: ptr_4k, ptr_2M, ptr_1G

    ptr_4k = hpp_alloc(N_4k * 1_size_t_kind, storage_size(N_4k) / 8_size_t_kind)
    ptr_2M = hpp_alloc(N_2M * 1_size_t_kind, storage_size(N_2M) / 8_size_t_kind)
    ptr_1G = hpp_alloc(N_1G * 1_size_t_kind, storage_size(N_1G) / 8_size_t_kind)

    write (*,*) "+++free"

    call hpp_free(ptr_4k)
    call hpp_free(ptr_2M)
    call hpp_free(ptr_1G)

end program
