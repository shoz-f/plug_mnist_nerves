defmodule PlugMnistTest do
  use ExUnit.Case
  doctest PlugMnist

  test "greets the world" do
    assert PlugMnist.hello() == :world
  end
end
