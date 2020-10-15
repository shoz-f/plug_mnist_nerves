defmodule PlugMnist.Router do
  use Plug.Router
  
  alias PlugMnist.TflInterp

#  @img_file if Mix.target() == :host, do: Application.app_dir(:plug_mnist, "priv/img.jpeg"), else: "/root/img.jpeg" 
  @img_file "/root/img.jpeg" 

  plug Plug.Static.IndexHtml, at: "/"
  plug Plug.Static, at: "/", from: :plug_mnist

  plug :match
  plug Plug.Parsers, parsers: [:urlencoded, :json],
                     pass: ["text/*"],
                     json_decoder: Jason
  plug :dispatch
  
  post "/mnist" do
    "data:image/jpeg;base64," <> data = conn.params["img"]
    File.write(@img_file, Base.decode64!(data))

    ans =
      case TflInterp.predict(@img_file) do
        {:ok, probs}  -> Enum.max_by(probs, &elem(&1, 1)) |> elem(0)
        {:error, _msg} -> -1
        {:timeout}    -> -2
      end
    send_resp(conn, 200, Jason.encode!(%{"ans" => ans}))
  end

  match _ do
    IO.inspect(conn)
    send_resp(conn, 404, "Oops!")
  end
end
