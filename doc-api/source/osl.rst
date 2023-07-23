osl
===============

.. note:: `miniosl` is designed to be used from Python.  C++ interface is supposed to be used in very exceptional cases.

Basic datatype
--------------
see `basic-type.h`
      
.. doxygenenum:: osl::Player
    :project: osl

.. doxygenfunction:: osl::alt(Player)
    :project: osl

.. doxygenfunction:: osl::idx(Player)
    :project: osl

.. doxygenfunction:: osl::sign(Player)
    :project: osl

.. doxygenenum:: osl::Ptype
    :project: osl

.. doxygenfunction:: osl::is_basic(Ptype)
    :project: osl

.. doxygenfunction:: osl::unpromote(Ptype)
    :project: osl

.. doxygenenum:: osl::Direction
    :project: osl

.. doxygenfunction:: osl::is_base8(Direction)
    :project: osl

.. doxygenfunction:: osl::is_basic(Direction)
    :project: osl

.. doxygenclass:: osl::Square
    :project: osl
    :members:

.. doxygenclass:: osl::Move
    :project: osl
    :members:

.. doxygenclass:: osl::Piece
    :project: osl
    :members:


State
--------------
see `base-state.h` and `state.h`

.. doxygenclass:: osl::BaseState
    :project: osl
    :members:

.. doxygenclass:: osl::EffectState
    :project: osl
    :members:

Game record
--------------
see also `record.h`

.. doxygenenum:: osl::GameResult
    :project: osl

.. doxygenfunction:: osl::win_result(Player)
    :project: osl

.. doxygenfunction:: osl::has_winner(GameResult)
    :project: osl

.. doxygenfunction:: osl::flip(GameResult)
    :project: osl

.. doxygenstruct:: osl::MiniRecord
    :project: osl
    :members:

.. doxygenfunction:: osl::csa::read_record(std::istream&)
    :project: osl

.. doxygenfunction:: osl::csa::read_board(const std::string&)
    :project: osl

.. doxygenfunction:: osl::usi::read_record(std::string)
    :project: osl

.. doxygenfunction:: osl::usi::to_state(const std::string&)
    :project: osl

.. doxygenfunction:: osl::to_ki2(Move, const EffectState&, Square)
    :project: osl

.. doxygenfunction:: osl::kanji::to_move(std::u8string, const EffectState&, Square last_to)
    :project: osl

.. doxygenstruct:: osl::GameManager
    :project: osl
    :members:

.. doxygenstruct:: osl::SubRecord
    :project: osl
    :members:
