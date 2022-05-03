/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.math.matview.h
*  PURPOSE:     Matview submodule for matrix operations.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

/*
    The matview submodule of the math module.

    This is a beast of its own.
*/

#ifndef _RENDERWARE_MATH_MATVIEW_
#define _RENDERWARE_MATH_MATVIEW_

namespace mview
{

#define MATVIEW_GENERAL_PROPS   size_t x, size_t y
#define MATVIEW_GENERAL_IMPL    x, y

    // Matrix views.
    template <MATVIEW_GENERAL_PROPS, typename... next>
    struct matview
    {
        static constexpr size_t x_pos = x;
        static constexpr size_t y_pos = y;
    };

#define MATVIEW_SKIPTHRESH_PROPS    size_t thresh_x, size_t thresh_y, size_t blockadd_x, size_t blockadd_y
#define MATVIEW_SKIPTHRESH_IMPL     thresh_x, thresh_y, blockadd_x, blockadd_y

    template <MATVIEW_SKIPTHRESH_PROPS>
    struct matview_thresh_updateskip
    {};

#define MATVIEW_SKIPPARAM_X     size_t block_x
#define MATVIEW_SKIPPARAM_Y     size_t block_y
#define MATVIEW_SKIPPARAM       size_t block_x, size_t block_y

#define MATVIEW_SKIPPROP_X      MATVIEW_SKIPPARAM_X
#define MATVIEW_SKIPPROP_Y      MATVIEW_SKIPPARAM_Y
#define MATVIEW_SKIPPROP        MATVIEW_SKIPPARAM

#define MATVIEW_SKIPPARAMIMPL_X block_x
#define MATVIEW_SKIPPARAMIMPL_Y block_y
#define MATVIEW_SKIPPARAMIMPL   block_x, block_y

#define MATVIEW_SKIPIMPL_X      MATVIEW_SKIPPARAMIMPL_X
#define MATVIEW_SKIPIMPL_Y      MATVIEW_SKIPPARAMIMPL_Y
#define MATVIEW_SKIPIMPL        MATVIEW_SKIPPARAMIMPL

    template <MATVIEW_SKIPPARAM>
    struct matview_maybe_skip
    {};

    template <MATVIEW_SKIPPARAM_X>
    struct matview_maybe_skip_x
    {};

    template <MATVIEW_SKIPPARAM_Y>
    struct matview_maybe_skip_y
    {};

    // We can validate maybe-skips into regular skips.
    struct matview_skip_x_validate
    {};

    struct matview_skip_y_validate
    {};

    struct matview_skip_validate
    {};

    struct matview_skip_validate_end
    {};

    template <MATVIEW_SKIPPARAM>
    struct matview_skip
    {};

    // Template math helpers.
    template <size_t num, size_t num2>
    struct matview_isgreaterequal
    {
        static constexpr bool value = ( num >= num2 );
    };

    template <bool b, size_t num1, size_t num2>
    struct matview_num_cond
    {
        static constexpr size_t value = num1;
    };

    template <size_t num1, size_t num2>
    struct matview_num_cond <false, num1, num2>
    {
        static constexpr size_t value = num2;
    };

    template <size_t left, size_t right, size_t addLeft, template <size_t num1, size_t num2> class cond>
    struct matview_cond_add : public matview_num_cond <cond <left, right>::value, left + addLeft, left>
    {};

    // This is some heavy shit.
    // Constructs a new matview from a matview state.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, bool hasDoneX, bool hasDoneY, typename... next>
    struct matview_skip_metaconstructor
    {};

    // NOT UPDATE MIRROR.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, typename... next>
    struct matview_skip_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL, false, false, next...>
    {
        typedef
            matview <x, y,
                     next...,
                     matview_maybe_skip <block_x, block_y>> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, typename... next>
    struct matview_skip_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL, true, false, next...>
    {
        typedef
            matview <x + 1, y,
                     matview_skip_x_validate, next..., matview_skip_validate_end,
                     matview_maybe_skip_y <block_y>> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, typename... next>
    struct matview_skip_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL, false, true, next...>
    {
        typedef
            matview <x, y + 1,
                     matview_skip_y_validate, next..., matview_skip_validate_end,
                     matview_maybe_skip_x <block_x>> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, typename... next>
    struct matview_skip_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL, true, true, next...>
    {
        typedef
            matview <x + 1, y + 1,
                     matview_skip_validate, next..., matview_skip_validate_end> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview_skip_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL,
                                               matview_isgreaterequal <x, block_x>::value,
                                               matview_isgreaterequal <y, block_y>::value,
                                               next...>::new_view
    {};

    template <MATVIEW_SKIPPARAM_Y>
    struct matview_skip_y
    {};

    // Construct a new matview from a matview state, for skip_y.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_Y, bool hasDoneY, typename... next>
    struct matview_skip_y_metaconstructor
    {};

    // NOT UPDATE MIRROR.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_Y, typename... next>
    struct matview_skip_y_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_Y, true, next...>
    {
        typedef
            matview <x, y + 1,
                     matview_skip_y_validate, next..., matview_skip_validate_end> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_Y, typename... next>
    struct matview_skip_y_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_Y, false, next...>
    {
        typedef
            matview <x, y,
                     next...,
                     matview_maybe_skip_y <block_y>> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_Y, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview_skip_y_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_Y,
                                                 matview_isgreaterequal <y, block_y>::value,
                                                 next...>::new_view
    {};

    template <MATVIEW_SKIPPARAM_X>
    struct matview_skip_x
    {};

    // Construct a new matview from a matview state, for skip_x.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_X, bool hasDoneX, typename... next>
    struct matview_skip_x_metaconstructor
    {};

    // NOT UPDATE MIRROR.
    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_X, typename... next>
    struct matview_skip_x_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_X, true, next...>
    {
        typedef
            matview <x + 1, y,
                     matview_skip_x_validate, next..., matview_skip_validate_end> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_X, typename... next>
    struct matview_skip_x_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_X, false, next...>
    {
        typedef
            matview <x, y,
                     next...,
                     matview_maybe_skip_x <block_x>> new_view;
    };

    template <MATVIEW_GENERAL_PROPS, MATVIEW_SKIPPROP_X, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview_skip_x_metaconstructor <MATVIEW_GENERAL_IMPL, MATVIEW_SKIPIMPL_X,
                                                 matview_isgreaterequal <x, block_x>::value,
                                                 next...>::new_view
    {};

    // If a maybe-skip was not determined at time of encounter, we remove it.
    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_maybe_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    // If a skip has been performed, previously not triggered skips can be asked to trigger.
    // We must turn all relevant maybe-skips into regular skips then.
    // This is called validation.
    // A maybe token has emitted an update to skips after it.
    // That is why we do not want it to emit any more updates, hence we set the flag.
    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_skip <MATVIEW_SKIPIMPL>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_maybe_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>, matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_x_validate, matview_skip_validate_end, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    // Same thing for y validation.
    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_skip <MATVIEW_SKIPIMPL>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_maybe_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, next..., matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>, matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_y_validate, matview_skip_validate_end, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    // And finally some special sauce for full validation.
    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_maybe_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_X>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_skip_x <MATVIEW_SKIPIMPL_X>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip_x <MATVIEW_SKIPIMPL_X>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP_Y>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_maybe_skip_y <MATVIEW_SKIPIMPL_Y>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip_y <MATVIEW_SKIPIMPL_Y>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip <MATVIEW_SKIPIMPL>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next, MATVIEW_SKIPPROP>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_maybe_skip <MATVIEW_SKIPIMPL>, next...>
        : public matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, next..., matview_skip <MATVIEW_SKIPIMPL>>
    {};

    template <MATVIEW_GENERAL_PROPS, typename... next>
    struct matview <MATVIEW_GENERAL_IMPL, matview_skip_validate, matview_skip_validate_end, next...>
        : public matview <MATVIEW_GENERAL_IMPL, next...>
    {};

    // Iterative building of matview skips.
    template <size_t rel_x, typename... cmdlist>
    struct matview_skip_x_constructor
    {
        typedef matview <rel_x, 0, cmdlist...> cstr_view;

        typedef matview_skip_x <cstr_view::x> type;
    };

    template <size_t rel_y, typename... cmdlist>
    struct matview_skip_y_constructor
    {
        typedef matview <0, rel_y, cmdlist...> cstr_view;

        typedef matview_skip_y <cstr_view::y> type;
    };

    // Example module: calculation of determinants for NxN matrix.
    template <typename numberType, typename matrixType, size_t detdimm, typename... viewmod>
    struct detcalc
    {
        template <size_t subdetdimm, bool isPositive, size_t iter, typename... subviewmod>
        struct detcalc_iter
        {
            AINLINE static numberType calc( const matrixType& mat )
            {
                numberType ourval = detcalc <numberType, matrixType, subdetdimm, subviewmod..., typename matview_skip_x_constructor <iter, subviewmod...>::type>::calc( mat );

                typedef matview <iter, 0, viewmod...> curview;

                ourval *= mat[ curview::y ][ curview::x ];

                numberType nextval = detcalc_iter <subdetdimm, !isPositive, iter + 1, subviewmod...>::calc( mat );

                if ( isPositive )
                {
                    return ( nextval + ourval );
                }

                return ( nextval - ourval );
            }
        };

        template <size_t subdetdimm, bool isPositive, typename... subviewmod>
        struct detcalc_iter <subdetdimm, isPositive, subdetdimm, subviewmod...>
        {
            AINLINE static numberType calc( const matrixType& mat )
            {
                numberType val = detcalc <numberType, matrixType, subdetdimm, subviewmod..., typename matview_skip_x_constructor <subdetdimm, subviewmod...>::type>::calc( mat );

                typedef matview <subdetdimm, 0, viewmod...> curview;

                val *= mat[ curview::y ][ curview::x ];

                if ( !isPositive )
                {
                    val = -val;
                }

                return val;
            }
        };

        AINLINE static numberType calc( const matrixType& mat )
        {
            return ( detcalc_iter <detdimm - 1, true, 0, viewmod..., typename matview_skip_y_constructor <0, viewmod...>::type>::calc( mat ) );
        }
    };

    template <typename numberType, typename matrixType, typename... viewmod>
    struct detcalc <numberType, matrixType, 2, viewmod...>
    {
        AINLINE static numberType calc( const matrixType& mat )
        {
            typedef matview <0, 0, viewmod...> top_left_view;
            typedef matview <1, 1, viewmod...> bottom_right_view;

            typedef matview <1, 0, viewmod...> top_right_view;
            typedef matview <0, 1, viewmod...> bottom_left_view;

            return (
                mat[ top_left_view::y ][ top_left_view::x ] * mat[ bottom_right_view::y ][ bottom_right_view::x ]
                -
                mat[ top_right_view::y ][ top_right_view::x ] * mat[ bottom_left_view::y ][ bottom_left_view::x ]
                );
        }
    };

    template <typename numberType, typename matrixType, typename... viewmod>
    struct detcalc <numberType, matrixType, 1, viewmod...>
    {
        AINLINE static numberType calc( const matrixType& mat )
        {
            typedef matview <0, 0, viewmod...> item_view;

            return mat[ item_view::y ][ item_view::x ];
        }
    };
};

#endif //_RENDERWARE_MATH_MATVIEW_
