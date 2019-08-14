module type Config = {
  let query: string;
  type t;
  let parse: Js.Json.t => t;
};

type error = {. "message": string};

type variant('a) =
  | Data('a)
  | Error(error)
  | Loading
  | NoData;

/**
 *
 *  https://github.com/apollographql/apollo-client/blob/master/packages/apollo-client/src/core/ObservableQuery.ts#L46
 */
[@bs.deriving abstract]
type updateQueryOptions = {
  [@bs.optional]
  fetchMoreResult: Js.Json.t,
  [@bs.optional]
  variables: Js.Json.t,
};

type updateQueryT = (Js.Json.t, updateQueryOptions) => Js.Json.t;

type refetch('a) = (~variables: Js.Json.t=?, unit) => Js.Promise.t('a);
type result('a) = {
  data: option('a),
  loading: bool,
  error: option(error),
  refetch: refetch('a),
  fetchMore:
    (~variables: Js.Json.t=?, ~updateQuery: updateQueryT, unit) =>
    Js.Promise.t(unit),
  networkStatus: Types.networkStatus,
};

/**
 * https://github.com/apollographql/apollo-client/blob/master/packages/apollo-client/src/core/watchQueryOptions.ts#L121
 */
[@bs.deriving abstract]
type fetchMoreOptions = {
  [@bs.optional]
  variables: Js.Json.t,
  updateQuery: updateQueryT,
};

module Make = (Config: Config) => {
  [@bs.module] external gql: ReasonApolloTypes.gql = "graphql-tag";

  [@bs.deriving abstract]
  type options = {
    [@bs.optional]
    variables: Js.Json.t,
    [@bs.optional]
    client: ApolloClient.generatedApolloClient,
    [@bs.optional]
    notifyOnNetworkStatusChange: bool,
  };

  [@bs.module "@apollo/react-hooks"]
  external useQuery:
    (ReasonApolloTypes.queryString, options) =>
    {
      .
      "data": Js.Nullable.t(Js.Json.t),
      "loading": bool,
      "error": Js.Nullable.t(error),
      [@bs.meth]
      "refetch": Js.Nullable.t(Js.Json.t) => Js.Promise.t(Js.Json.t),
      [@bs.meth] "fetchMore": fetchMoreOptions => Js.Promise.t(unit),
      "networkStatus": Js.Nullable.t(int),
    } =
    "useQuery";

  let use = (~variables=?, ~client=?, ~notifyOnNetworkStatusChange=?, ()) => {
    let jsResult =
      useQuery(
        gql(. Config.query),
        options(~variables?, ~client?, ~notifyOnNetworkStatusChange?, ()),
      );

    let result = {
      data:
        jsResult##data
        ->Js.Nullable.toOption
        ->Belt.Option.flatMap(data =>
            switch (Config.parse(data)) {
            | parsedData => Some(parsedData)
            | exception _ => None
            }
          ),
      loading: jsResult##loading,
      error: jsResult##error->Js.Nullable.toOption,
      networkStatus: Types.toNetworkStatus(jsResult##networkStatus),
      refetch: (~variables=?, ()) =>
        jsResult##refetch(Js.Nullable.fromOption(variables))
        |> Js.Promise.then_(result =>
             Config.parse(result) |> Js.Promise.resolve
           ),
      fetchMore: (~variables=?, ~updateQuery, ()) =>
        jsResult##fetchMore(fetchMoreOptions(~variables?, ~updateQuery, ())),
    };

    (
      switch (result) {
      | {loading: true} => Loading
      | {error: Some(error)} => Error(error)
      | {data: Some(data)} => Data(data)
      | _ => NoData
      },
      result,
    );
  };
};
