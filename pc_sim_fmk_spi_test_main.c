#if defined(FMKSPI_TEST_BACKEND) && !defined(PIO_UNIT_TESTING)
/*********************************
 * main
 *********************************/
int main(void)
{
    int Ret_s32 = 0;

    //---- 1- Provide a linkable no-operation executable for pio run ----//
    return Ret_s32;
}
#endif
