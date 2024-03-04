"""
	A lightweight wrapper around psycopg3.
"""

import logging
import psycopg
import time

# Setup logging
log = logging.getLogger("location.database")

class Connection(object):
	def __init__(self, host, database, user=None, password=None):
		# Create a connection pool
		self.connection = psycopg.connect(
			"postgresql://%s:%s@%s/%s" % (user, password, host, database),

			# Enable autocommit
			autocommit=True,

			# Return any rows as dicts
			row_factory = psycopg.rows.dict_row,
		)

	def _execute(self, cursor, execute, query, parameters):
		# Store the time we started this query
		#t = time.monotonic()

		#try:
		#	log.debug("Running SQL query %s" % (query % parameters))
		#except Exception:
		#	pass

		# Execute the query
		execute(query, parameters)

		# How long did this take?
		#elapsed = time.monotonic() - t

		# Log the query time
		#log.debug("  Query time: %.2fms" % (elapsed * 1000))

	def query(self, query, *parameters, **kwparameters):
		"""
			Returns a row list for the given query and parameters.
		"""
		with self.connection.cursor() as cursor:
			self._execute(cursor, cursor.execute, query, parameters or kwparameters)

			return [Row(row) for row in cursor]

	def get(self, query, *parameters, **kwparameters):
		"""
			Returns the first row returned for the given query.
		"""
		rows = self.query(query, *parameters, **kwparameters)
		if not rows:
			return None
		elif len(rows) > 1:
			raise Exception("Multiple rows returned for Database.get() query")
		else:
			return rows[0]

	def execute(self, query, *parameters, **kwparameters):
		"""
			Executes the given query.
		"""
		with self.connection.cursor() as cursor:
			self._execute(cursor, cursor.execute, query, parameters or kwparameters)

	def executemany(self, query, parameters):
		"""
			Executes the given query against all the given param sequences.
		"""
		with self.connection.cursor() as cursor:
			self._execute(cursor, cursor.executemany, query, parameters)

	def transaction(self):
		"""
			Creates a new transaction on the current tasks' connection
		"""
		return self.connection.transaction()


class Row(dict):
	"""A dict that allows for object-like property access syntax."""
	def __getattr__(self, name):
		try:
			return self[name]
		except KeyError:
			raise AttributeError(name)
