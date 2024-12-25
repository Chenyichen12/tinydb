create table student (id long, name varchar(255), age int, sex int);

select * from student;

select * from student_apartment;

select * from student, student_apartment where student.id = student_apartment.id;

select student.id, student.name, student_apartment.apartment from student, student_apartment where student.id = student_apartment.id;

insert into student (id, name, age, sex) values (5555, '伊子米', 18, true);

select * from student limit 1;

select * from student order by student.id;

select * from student order by student.name;

update student set name='test' where student.age=18;

delete from student where student.age = 18;


some benchmark test:

insert amount: 100000 Time: 1396ms
select seq amount: 100000 Time: 385ms
select id amount: 100000 Time: 441ms
update amount: 10000 Time: 2257ms