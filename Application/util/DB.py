import mysql.connector

class DBAccess:
    def __init__(self, host, user, password, database):
        # Database connection parameters
        self.host = host
        self.user = user
        self.password = password
        self.database = database

        self.mydb = None      # MySQL connection object
        self.mycursor = None  # Cursor object for executing queries

    def connect(self):
        """
        Establishes a connection to the MySQL database.
        Returns True if the connection is successful, otherwise returns None.
        """
        try:
            self.mydb = mysql.connector.connect(
                host=self.host,
                user=self.user,
                password=self.password,
                database=self.database
            )
            self.mycursor = self.mydb.cursor()
        except Exception as e:
            print(f"DB: connection error: {e}")
            return None
        return True

    def close(self):
        """
        Closes the connection to the database and resets references.
        """
        if self.mydb:
            self.mydb.close()
        self.mydb = None
        self.mycursor = None

    def query(self, query, val, fetchall=False):
        """
        Executes an SQL query on the connected database.

        Parameters:
        - `query`: SQL query string with placeholders
        - `val`: Tuple of values to bind to the placeholders
        - `fetchall`: If True, returns fetched results (for SELECT queries),
                     otherwise commits the transaction (for INSERT/UPDATE)

        Returns:
        - List of results if `fetchall` is True
        - True if the query executed successfully (non-SELECT queries)
        - None if an error occurred
        """
        if self.mydb is None:
            print("DB: Database connection not established.")
            return None

        try:
            self.mycursor.execute(query, val)
        except Exception as e:
            print(f"DB: Query execution error: {e}")
            return None

        if fetchall:
            return self.mycursor.fetchall()
        else:
            self.mydb.commit()
            return True
