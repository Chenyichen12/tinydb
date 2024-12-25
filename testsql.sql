

create table student (id long, name varchar(255), age int, sex int);

select * from student order by student.name;

select * from student, student_apartment where student.id = student_apartment.id;

select student.id, student.name, student_apartment.apartment from student, student_apartment where student.id = student_apartment.id;

insert into student (id, name, age, sex) values (5555, '伊子米', 18, true);

delete from student where student.age = 18;