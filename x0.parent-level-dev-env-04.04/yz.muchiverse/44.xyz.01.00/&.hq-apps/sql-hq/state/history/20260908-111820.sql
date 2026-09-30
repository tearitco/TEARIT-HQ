SELECT city, COUNT(*) AS n FROM people GROUP BY city ORDER BY n DESC, city;
